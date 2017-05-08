/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk/msm-clock-generic.h>

#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/clock-rpm.h>

#include <dt-bindings/clock/msm-clocks-8996.h>
#include <dt-bindings/clock/msm-clocks-hwio-8996.h>

#include "reset.h"
#include "vdd-level-8996.h"

static void __iomem *virt_base;
static void __iomem *virt_dbgbase;

#define cxo_clk_src_source_val 0
#define cxo_clk_src_ao_source_val 0
#define gpll0_out_main_source_val 1
#define gpll4_out_main_source_val 5

#define FIXDIV(div) (div ? (2 * (div) - 1) : (0))

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)FIXDIV(div)) \
			| BVAL(10, 8, s##_source_val), \
	}

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

DEFINE_EXT_CLK(mmss_gcc_dbg_clk, NULL);
DEFINE_EXT_CLK(gpu_gcc_dbg_clk, NULL);
DEFINE_EXT_CLK(cpu_dbg_clk, NULL);
DEFINE_CLK_RPM_SMD_BRANCH(cxo_clk_src, cxo_clk_src_ao, RPM_MISC_CLK_TYPE,
				CXO_CLK_SRC_ID, 19200000);
DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_CLK_ID, NULL);

DEFINE_CLK_RPM_SMD(mmssnoc_axi_rpm_clk, mmssnoc_axi_rpm_a_clk,
				RPM_MMAXI_CLK_TYPE, MMXI_CLK_ID, NULL);
DEFINE_CLK_VOTER(mmssnoc_axi_clk, &mmssnoc_axi_rpm_clk.c, 0);
DEFINE_CLK_VOTER(mmssnoc_axi_a_clk, &mmssnoc_axi_rpm_a_clk.c, 0);
DEFINE_CLK_VOTER(mmssnoc_gds_clk, &mmssnoc_axi_rpm_clk.c, 40000000);

DEFINE_CLK_RPM_SMD_BRANCH(aggre1_noc_clk, aggre1_noc_a_clk,
				RPM_AGGR_CLK_TYPE, AGGR1_NOC_ID, 1000);
DEFINE_CLK_RPM_SMD_BRANCH(aggre2_noc_clk, aggre2_noc_a_clk,
				RPM_AGGR_CLK_TYPE, AGGR2_NOC_ID, 1000);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk1, bb_clk1_ao, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk1_pin, bb_clk1_pin_ao,
					BB_CLK1_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk2, bb_clk2_ao, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk2_pin, bb_clk2_pin_ao,
					BB_CLK2_PIN_ID);
static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, &cnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, &cnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_ssc_clk, &cxo_clk_src.c);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_clk1_ao, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_clk2_ao, DIV_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk3, div_clk3_ao, DIV_CLK3_ID);
DEFINE_CLK_RPM_SMD(ipa_clk, ipa_a_clk, RPM_IPA_CLK_TYPE,
					IPA_CLK_ID, NULL);

DEFINE_CLK_RPM_SMD(ce1_clk, ce1_a_clk, RPM_CE_CLK_TYPE,
					CE1_CLK_ID, NULL);
DEFINE_CLK_DUMMY(gcc_ce1_ahb_m_clk, 0);
DEFINE_CLK_DUMMY(gcc_ce1_axi_m_clk, 0);
DEFINE_CLK_DUMMY(measure_only_bimc_hmss_axi_clk, 0);

DEFINE_CLK_RPM_SMD_XO_BUFFER(ln_bb_clk, ln_bb_a_clk, LN_BB_CLK_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(ln_bb_clk_pin, ln_bb_a_clk_pin,
				LN_BB_CLK_PIN_ID);
static DEFINE_CLK_VOTER(mcd_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_pm_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc_clk.c, 0);
static DEFINE_CLK_VOTER(qcedev_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcrypto_ce1_clk, &ce1_clk.c, 85710000);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE,
			QDSS_CLK_ID);
static DEFINE_CLK_VOTER(qseecom_ce1_clk, &ce1_clk.c, 85710000);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk1, rf_clk1_ao, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk1_pin, rf_clk1_pin_ao,
				RF_CLK1_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk2, rf_clk2_ao, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk2_pin, rf_clk2_pin_ao,
				RF_CLK2_PIN_ID);
static DEFINE_CLK_VOTER(scm_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0 = {
	.en_reg = (void __iomem *)GCC_APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GCC_GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_base,
	.c = {
		.rate = 600000000,
		.parent = &cxo_clk_src.c,
		.dbg_name = "gpll0",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0.c),
	},
};

static struct pll_vote_clk gpll0_ao = {
	.en_reg = (void __iomem *)GCC_APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GCC_GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_base,
	.c = {
		.rate = 600000000,
		.parent = &cxo_clk_src_ao.c,
		.dbg_name = "gpll0_ao",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao.c),
	},
};

DEFINE_EXT_CLK(gpll0_out_main, &gpll0.c);

static struct local_vote_clk gcc_mmss_gpll0_div_clk = {
	.vote_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE_1,
	.cbcr_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE_1,
	.en_mask = BIT(0),
	.base = &virt_base,
	.halt_check = DELAY,
	.c = {
		.dbg_name = "gcc_mmss_gpll0_div_clk",
		.parent = &gpll0.c,
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_mmss_gpll0_div_clk.c),
	},
};

static struct pll_vote_clk gpll4 = {
	.en_reg = (void __iomem *)GCC_APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GCC_GPLL4_MODE,
	.status_mask = BIT(30),
	.base = &virt_base,
	.c = {
		.rate = 384000000,
		.parent = &cxo_clk_src.c,
		.dbg_name = "gpll4",
		.ops = &clk_ops_pll_vote,
		VDD_DIG_FMAX_MAP1(NOMINAL, 384000000),
		CLK_INIT(gpll4.c),
	},
};

DEFINE_EXT_CLK(gpll4_out_main, &gpll4.c);

static struct clk_freq_tbl ftbl_ufs_axi_clk_src[] = {
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F( 240000000, gpll0_out_main,  2.5,    0,     0),
	F_END
};

static struct rcg_clk ufs_axi_clk_src = {
	.cmd_rcgr_reg = GCC_UFS_AXI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_ufs_axi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "ufs_axi_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP4(LOWER, 19200000, LOW, 100000000,
				NOMINAL, 200000000, HIGH, 240000000),
		CLK_INIT(ufs_axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pcie_aux_clk_src[] = {
	F(   1010526,   cxo_clk_src,   1,    1,   19),
	F_END
};

static struct rcg_clk pcie_aux_clk_src = {
	.cmd_rcgr_reg = GCC_PCIE_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_pcie_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOWER, 1011000),
		CLK_INIT(pcie_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb30_master_clk_src[] = {
	F(  19200000,    cxo_clk_src,    1,    0,    0),
	F( 120000000, gpll0_out_main,    5,    0,    0),
	F( 150000000, gpll0_out_main,    4,    0,    0),
	F_END
};

static struct rcg_clk usb30_master_clk_src = {
	.cmd_rcgr_reg = GCC_USB30_MASTER_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_usb30_master_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb30_master_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 60000000, LOW, 120000000,
						NOMINAL, 150000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb20_master_clk_src[] = {
	F( 120000000, gpll0_out_main,    5,    0,     0),
	F_END
};

static struct rcg_clk usb20_master_clk_src = {
	.cmd_rcgr_reg = GCC_USB20_MASTER_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_usb20_master_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb20_master_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 60000000,
						NOMINAL, 120000000),
		CLK_INIT(usb20_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ufs_ice_core_clk_src[] = {
	F( 19200000, cxo_clk_src,    1,    0,     0),
	F( 150000000, gpll0_out_main, 4,   0,     0),
	F( 300000000, gpll0_out_main, 2,   0,     0),
	F_END
};

static struct rcg_clk ufs_ice_core_clk_src = {
	.cmd_rcgr_reg = GCC_UFS_ICE_CORE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ufs_ice_core_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "ufs_ice_core_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 150000000,
						NOMINAL, 300000000),
		CLK_INIT(ufs_ice_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup1_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp1_qup1_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup1_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup1_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup2_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp1_qup2_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup2_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup2_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup3_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp1_qup3_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup3_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup3_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup4_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp1_qup4_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup4_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup4_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup5_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp1_qup5_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup5_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup5_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup6_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp1_qup6_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup6_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup6_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_uart1_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_uart1_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_uart2_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_uart2_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_uart3_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_uart3_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_uart4_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_uart4_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_uart5_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart5_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_uart5_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp1_uart5_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_uart6_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart6_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_uart6_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp1_uart6_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup1_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp2_qup1_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup1_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_qup1_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp2_qup1_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup2_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp2_qup2_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup2_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup2_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_qup2_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp2_qup2_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup3_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp2_qup3_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup3_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup3_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_qup3_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp2_qup3_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup4_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp2_qup4_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup4_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup4_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_qup4_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp2_qup4_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup5_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp2_qup5_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup5_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup5_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_qup5_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp2_qup5_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup6_i2c_apps_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp2_qup6_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup6_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_qup6_spi_apps_clk_src[] = {
	F(    960000,         cxo_clk_src,   10,    1,     2),
	F(   4800000,         cxo_clk_src,    4,    0,     0),
	F(   9600000,         cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp2_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_qup6_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 25000000,
						NOMINAL, 50000000),
		CLK_INIT(blsp2_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_uart1_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp2_uart1_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_uart1_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp2_uart1_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_uart2_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp2_uart2_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_uart2_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp2_uart2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_uart3_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp2_uart3_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_uart3_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp2_uart3_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_uart4_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp2_uart4_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_uart4_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp2_uart4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_uart5_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp2_uart5_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_uart5_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp2_uart5_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp2_uart6_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63157895, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp2_uart6_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp2_uart6_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 31580000,
						NOMINAL, 63160000),
		CLK_INIT(blsp2_uart6_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp1_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GCC_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
						NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp2_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GCC_GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
						NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp3_clk_src[] = {
	F(  19200000,         cxo_clk_src,    1,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GCC_GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp3_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
						NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_hmss_rbcpr_clk_src[] = {
	F(  19200000,     cxo_clk_src_ao,    1,    0,     0),
	F_END
};

static struct rcg_clk hmss_rbcpr_clk_src = {
	.cmd_rcgr_reg = GCC_HMSS_RBCPR_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_hmss_rbcpr_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "hmss_rbcpr_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(hmss_rbcpr_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pdm2_clk_src[] = {
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F_END
};

static struct rcg_clk pdm2_clk_src = {
	.cmd_rcgr_reg = GCC_PDM2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_pdm2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pdm2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 60000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_qspi_ser_clk_src[] = {
	F(  75000000,  gpll0_out_main,    8,    0,     0),
	F( 150000000,  gpll0_out_main,    4,    0,     0),
	F( 256000000,  gpll4_out_main,  1.5,    0,     0),
	F( 300000000,  gpll0_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk qspi_ser_clk_src = {
	.cmd_rcgr_reg = GCC_QSPI_SER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_qspi_ser_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "qspi_ser_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 80200000, LOW, 160400000,
							NOMINAL, 320000000),
		CLK_INIT(qspi_ser_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc1_apps_clk_src[] = {
	F(    144000,     cxo_clk_src,   16,    3,    25),
	F(    400000,     cxo_clk_src,   12,    1,     4),
	F(  20000000,  gpll0_out_main,   15,    1,     2),
	F(  25000000,  gpll0_out_main,   12,    1,     2),
	F(  50000000,  gpll0_out_main,   12,    0,     0),
	F(  96000000,  gpll4_out_main,    4,    0,     0),
	F( 192000000,  gpll4_out_main,    2,    0,     0),
	F( 384000000,  gpll4_out_main,    1,    0,     0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = GCC_SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc1_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 200000000,
						NOMINAL, 400000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc1_ice_core_clk_src[] = {
	F(  19200000,     cxo_clk_src,    1,    0,     0),
	F( 150000000,  gpll0_out_main,    4,    0,     0),
	F( 300000000,  gpll0_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk sdcc1_ice_core_clk_src = {
	.cmd_rcgr_reg = GCC_SDCC1_ICE_CORE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_sdcc1_ice_core_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc1_ice_core_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 150000000,
						NOMINAL, 300000000),
		CLK_INIT(sdcc1_ice_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc2_apps_clk_src[] = {
	F(    144000,         cxo_clk_src,   16,    3,    25),
	F(    400000,         cxo_clk_src,   12,    1,     4),
	F(  20000000, gpll0_out_main,   15,    1,     2),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = GCC_SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc2_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 100000000,
						NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc3_apps_clk_src[] = {
	F(    144000,         cxo_clk_src,   16,    3,    25),
	F(    400000,         cxo_clk_src,   12,    1,     4),
	F(  20000000, gpll0_out_main,   15,    1,     2),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk sdcc3_apps_clk_src = {
	.cmd_rcgr_reg = GCC_SDCC3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc3_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 100000000,
						NOMINAL, 200000000),
		CLK_INIT(sdcc3_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc4_apps_clk_src[] = {
	F(    144000,         cxo_clk_src,   16,    3,    25),
	F(    400000,         cxo_clk_src,   12,    1,     4),
	F(  20000000, gpll0_out_main,   15,    1,     2),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F_END
};

static struct rcg_clk sdcc4_apps_clk_src = {
	.cmd_rcgr_reg = GCC_SDCC4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc4_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 50000000,
						NOMINAL, 100000000),
		CLK_INIT(sdcc4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_tsif_ref_clk_src[] = {
	F(    105495,         cxo_clk_src,    1,    1,   182),
	F_END
};

static struct rcg_clk tsif_ref_clk_src = {
	.cmd_rcgr_reg = GCC_TSIF_REF_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_tsif_ref_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "tsif_ref_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOWER, 105500),
		CLK_INIT(tsif_ref_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb20_mock_utmi_clk_src[] = {
	F(  19200000, cxo_clk_src,    1,    0,     0),
	F_END
};

static struct rcg_clk usb20_mock_utmi_clk_src = {
	.cmd_rcgr_reg = GCC_USB20_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb20_mock_utmi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb20_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 60000000),
		CLK_INIT(usb20_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb30_mock_utmi_clk_src[] = {
	F(  19200000, cxo_clk_src,    1,    0,     0),
	F_END
};

static struct rcg_clk usb30_mock_utmi_clk_src = {
	.cmd_rcgr_reg = GCC_USB30_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb30_mock_utmi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb30_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 40000000, LOW, 60000000),
		CLK_INIT(usb30_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb3_phy_aux_clk_src[] = {
	F(   1200000,         cxo_clk_src,   16,    0,     0),
	F_END
};

static struct rcg_clk usb3_phy_aux_clk_src = {
	.cmd_rcgr_reg = GCC_USB3_PHY_AUX_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb3_phy_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb3_phy_aux_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 1200000),
		CLK_INIT(usb3_phy_aux_clk_src.c),
	},
};

static struct reset_clk gcc_qusb2phy_prim_reset = {
	.reset_reg = GCC_QUSB2PHY_PRIM_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_qusb2phy_prim_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_qusb2phy_prim_reset.c),
	},
};

static struct reset_clk gcc_qusb2phy_sec_reset = {
	.reset_reg = GCC_QUSB2PHY_SEC_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_qusb2phy_sec_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_qusb2phy_sec_reset.c),
	},
};

static struct branch_clk gcc_periph_noc_usb20_ahb_clk = {
	.cbcr_reg = GCC_PERIPH_NOC_USB20_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_periph_noc_usb20_ahb_clk",
		.parent = &usb20_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_periph_noc_usb20_ahb_clk.c),
	},
};

static struct branch_clk gcc_aggre0_cnoc_ahb_clk = {
	.cbcr_reg = GCC_AGGRE0_CNOC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre0_cnoc_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_aggre0_cnoc_ahb_clk.c),
	},
};

static struct branch_clk gcc_aggre0_snoc_axi_clk = {
	.cbcr_reg = GCC_AGGRE0_SNOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre0_snoc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_aggre0_snoc_axi_clk.c),
	},
};

static struct branch_clk gcc_smmu_aggre0_ahb_clk = {
	.cbcr_reg = GCC_SMMU_AGGRE0_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "gcc_smmu_aggre0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_smmu_aggre0_ahb_clk.c),
	},
};

static struct branch_clk gcc_smmu_aggre0_axi_clk = {
	.cbcr_reg = GCC_SMMU_AGGRE0_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "gcc_smmu_aggre0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_smmu_aggre0_axi_clk.c),
	},
};

static struct gate_clk gcc_aggre0_noc_qosgen_extref_clk = {
	.en_reg = GCC_AGGRE0_NOC_QOSGEN_EXTREF_CTL,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre0_noc_qosgen_extref_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_aggre0_noc_qosgen_extref_clk.c),
	},
};

static struct reset_clk gcc_pcie_0_phy_reset = {
	.reset_reg = GCC_PCIE_0_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_0_phy_reset.c),
	},
};

static struct gate_clk gcc_pcie_0_pipe_clk = {
	.en_reg = GCC_PCIE_0_PIPE_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_pipe_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_pcie_0_pipe_clk.c),
	},
};

static struct reset_clk gcc_pcie_1_phy_reset = {
	.reset_reg = GCC_PCIE_1_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_1_phy_reset.c),
	},
};

static struct gate_clk gcc_pcie_1_pipe_clk = {
	.en_reg = GCC_PCIE_1_PIPE_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_pipe_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_pcie_1_pipe_clk.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = GCC_BLSP1_AHB_CBCR,
	.bcr_reg = GCC_BLSP1_BCR,
	.vote_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(17),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.parent = &blsp1_qup1_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.parent = &blsp1_qup1_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.parent = &blsp1_qup2_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.parent = &blsp1_qup2_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.parent = &blsp1_qup3_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.parent = &blsp1_qup3_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.parent = &blsp1_qup4_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.parent = &blsp1_qup4_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.parent = &blsp1_qup5_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.parent = &blsp1_qup5_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.parent = &blsp1_qup6_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP1_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.parent = &blsp1_qup6_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart1_apps_clk = {
	.cbcr_reg = GCC_BLSP1_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.parent = &blsp1_uart1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart2_apps_clk = {
	.cbcr_reg = GCC_BLSP1_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.parent = &blsp1_uart2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart3_apps_clk = {
	.cbcr_reg = GCC_BLSP1_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.parent = &blsp1_uart3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart4_apps_clk = {
	.cbcr_reg = GCC_BLSP1_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.parent = &blsp1_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart5_apps_clk = {
	.cbcr_reg = GCC_BLSP1_UART5_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.parent = &blsp1_uart5_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart6_apps_clk = {
	.cbcr_reg = GCC_BLSP1_UART6_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
		.parent = &blsp1_uart6_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart6_apps_clk.c),
	},
};

static struct local_vote_clk gcc_blsp2_ahb_clk = {
	.cbcr_reg = GCC_BLSP2_AHB_CBCR,
	.bcr_reg = GCC_BLSP2_BCR,
	.vote_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(15),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp2_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.parent = &blsp2_qup1_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup1_spi_apps_clk",
		.parent = &blsp2_qup1_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.parent = &blsp2_qup2_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup2_spi_apps_clk",
		.parent = &blsp2_qup2_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.parent = &blsp2_qup3_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup3_spi_apps_clk",
		.parent = &blsp2_qup3_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.parent = &blsp2_qup4_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup4_spi_apps_clk",
		.parent = &blsp2_qup4_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.parent = &blsp2_qup5_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup5_spi_apps_clk",
		.parent = &blsp2_qup5_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_i2c_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.parent = &blsp2_qup6_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_spi_apps_clk = {
	.cbcr_reg = GCC_BLSP2_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_qup6_spi_apps_clk",
		.parent = &blsp2_qup6_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart1_apps_clk = {
	.cbcr_reg = GCC_BLSP2_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_uart1_apps_clk",
		.parent = &blsp2_uart1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart2_apps_clk = {
	.cbcr_reg = GCC_BLSP2_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_uart2_apps_clk",
		.parent = &blsp2_uart2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart3_apps_clk = {
	.cbcr_reg = GCC_BLSP2_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_uart3_apps_clk",
		.parent = &blsp2_uart3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart4_apps_clk = {
	.cbcr_reg = GCC_BLSP2_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_uart4_apps_clk",
		.parent = &blsp2_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart5_apps_clk = {
	.cbcr_reg = GCC_BLSP2_UART5_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_uart5_apps_clk",
		.parent = &blsp2_uart5_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart6_apps_clk = {
	.cbcr_reg = GCC_BLSP2_UART6_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.parent = &blsp2_uart6_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart6_apps_clk.c),
	},
};

static struct local_vote_clk gcc_boot_rom_ahb_clk = {
	.cbcr_reg = GCC_BOOT_ROM_AHB_CBCR,
	.bcr_reg = GCC_BOOT_ROM_BCR,
	.vote_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GCC_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gp1_clk",
		.parent = &gp1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GCC_GP2_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gp2_clk",
		.parent = &gp2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GCC_GP3_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gp3_clk",
		.parent = &gp3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_mmss_noc_cfg_ahb_clk = {
	.cbcr_reg = GCC_MMSS_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mmss_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_noc_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_aux_clk = {
	.cbcr_reg = GCC_PCIE_0_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_aux_clk",
		.parent = &pcie_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_cfg_ahb_clk = {
	.cbcr_reg = GCC_PCIE_0_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_mstr_axi_clk = {
	.cbcr_reg = GCC_PCIE_0_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_slv_axi_clk = {
	.cbcr_reg = GCC_PCIE_0_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_aux_clk = {
	.cbcr_reg = GCC_PCIE_1_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_aux_clk",
		.parent = &pcie_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_cfg_ahb_clk = {
	.cbcr_reg = GCC_PCIE_1_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_mstr_axi_clk = {
	.cbcr_reg = GCC_PCIE_1_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_slv_axi_clk = {
	.cbcr_reg = GCC_PCIE_1_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_2_aux_clk = {
	.cbcr_reg = GCC_PCIE_2_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_2_aux_clk",
		.parent = &pcie_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_2_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_2_cfg_ahb_clk = {
	.cbcr_reg = GCC_PCIE_2_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_2_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_2_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_2_mstr_axi_clk = {
	.cbcr_reg = GCC_PCIE_2_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_2_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_2_mstr_axi_clk.c),
	},
};

static struct reset_clk gcc_pcie_2_phy_reset = {
	.reset_reg = GCC_PCIE_2_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_2_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_2_phy_reset.c),
	},
};

static struct gate_clk gcc_pcie_2_pipe_clk = {
	.en_reg = GCC_PCIE_2_PIPE_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_2_pipe_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_pcie_2_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_2_slv_axi_clk = {
	.cbcr_reg = GCC_PCIE_2_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_2_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_2_slv_axi_clk.c),
	},
};

static struct reset_clk gcc_pcie_phy_reset = {
	.reset_reg = GCC_PCIE_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_phy_reset.c),
	},
};

static struct reset_clk gcc_pcie_phy_com_reset = {
	.reset_reg = GCC_PCIE_PHY_COM_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_phy_com_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_phy_com_reset.c),
	},
};

static struct reset_clk gcc_pcie_phy_nocsr_com_phy_reset = {
	.reset_reg = GCC_PCIE_PHY_NOCSR_COM_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_phy_nocsr_com_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_phy_nocsr_com_phy_reset.c),
	},
};

static struct branch_clk gcc_pcie_phy_aux_clk = {
	.cbcr_reg = GCC_PCIE_PHY_AUX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_phy_aux_clk",
		.parent = &pcie_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_phy_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_phy_cfg_ahb_clk = {
	.cbcr_reg = GCC_PCIE_PHY_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_phy_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_phy_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = GCC_PDM2_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pdm2_clk",
		.parent = &pdm2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm2_clk.c),
	},
};

static struct branch_clk gcc_pdm_ahb_clk = {
	.cbcr_reg = GCC_PDM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pdm_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_prng_ahb_clk = {
	.cbcr_reg = GCC_PRNG_AHB_CBCR,
	.bcr_reg = GCC_PRNG_BCR,
	.vote_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(13),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_prng_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_prng_ahb_clk.c),
	},
};

static struct branch_clk gcc_qspi_ahb_clk = {
	.cbcr_reg = GCC_QSPI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_qspi_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_qspi_ahb_clk.c),
	},
};

static struct branch_clk gcc_qspi_ser_clk = {
	.cbcr_reg = GCC_QSPI_SER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_qspi_ser_clk",
		.parent = &qspi_ser_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_qspi_ser_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_ahb_clk = {
	.cbcr_reg = GCC_SDCC1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_apps_clk = {
	.cbcr_reg = GCC_SDCC1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc1_apps_clk",
		.parent = &sdcc1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_ice_core_clk = {
	.cbcr_reg = GCC_SDCC1_ICE_CORE_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc1_ice_core_clk",
		.parent = &sdcc1_ice_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ice_core_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_ahb_clk = {
	.cbcr_reg = GCC_SDCC2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_apps_clk = {
	.cbcr_reg = GCC_SDCC2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc2_apps_clk",
		.parent = &sdcc2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_ahb_clk = {
	.cbcr_reg = GCC_SDCC3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_apps_clk = {
	.cbcr_reg = GCC_SDCC3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc3_apps_clk",
		.parent = &sdcc3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_ahb_clk = {
	.cbcr_reg = GCC_SDCC4_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc4_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_apps_clk = {
	.cbcr_reg = GCC_SDCC4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc4_apps_clk",
		.parent = &sdcc4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_apps_clk.c),
	},
};

static struct branch_clk gcc_tsif_ahb_clk = {
	.cbcr_reg = GCC_TSIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_tsif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ahb_clk.c),
	},
};

static struct branch_clk gcc_tsif_ref_clk = {
	.cbcr_reg = GCC_TSIF_REF_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_tsif_ref_clk",
		.parent = &tsif_ref_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ref_clk.c),
	},
};

static struct branch_clk gcc_ufs_ahb_clk = {
	.cbcr_reg = GCC_UFS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_ahb_clk.c),
	},
};

static struct branch_clk gcc_ufs_axi_clk = {
	.bcr_reg = GCC_UFS_BCR,
	.cbcr_reg = GCC_UFS_AXI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_axi_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_ufs_ice_core_clk = {
	.cbcr_reg = GCC_UFS_ICE_CORE_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_ice_core_clk",
		.parent = &ufs_ice_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_ice_core_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_cfg_branch_clk = {
	.cbcr_reg = GCC_UFS_RX_CFG_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_rx_cfg_branch_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_rx_cfg_branch_clk.c),
	},
};

DEFINE_FIXED_SLAVE_DIV_CLK(gcc_ufs_rx_cfg_clk, 16,
						&gcc_ufs_rx_cfg_branch_clk.c);

static struct gate_clk gcc_ufs_rx_symbol_0_clk = {
	.en_reg = GCC_UFS_RX_SYMBOL_0_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_rx_symbol_0_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_ufs_rx_symbol_0_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_symbol_1_clk = {
	.cbcr_reg = GCC_UFS_RX_SYMBOL_1_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_rx_symbol_1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_rx_symbol_1_clk.c),
	},
};

static struct branch_clk gcc_ufs_tx_cfg_branch_clk = {
	.cbcr_reg = GCC_UFS_TX_CFG_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_tx_cfg_branch_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_tx_cfg_branch_clk.c),
	},
};

DEFINE_FIXED_SLAVE_DIV_CLK(gcc_ufs_tx_cfg_clk, 16,
						&gcc_ufs_tx_cfg_branch_clk.c);

static struct gate_clk gcc_ufs_tx_symbol_0_clk = {
	.en_reg = GCC_UFS_TX_SYMBOL_0_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_tx_symbol_0_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_ufs_tx_symbol_0_clk.c),
	},
};

DEFINE_FIXED_DIV_CLK(gcc_ufs_ice_core_postdiv_clk_src, 2,
						&ufs_ice_core_clk_src.c);

static struct branch_clk gcc_ufs_unipro_core_clk = {
	.cbcr_reg = GCC_UFS_UNIPRO_CORE_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_unipro_core_clk",
		.parent = &gcc_ufs_ice_core_postdiv_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_unipro_core_clk.c),
	},
};

static struct gate_clk gcc_ufs_sys_clk_core_clk = {
	.en_reg = GCC_UFS_SYS_CLK_CORE_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_sys_clk_core_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_ufs_sys_clk_core_clk.c),
	},
};

static struct gate_clk gcc_ufs_tx_symbol_clk_core_clk = {
	.en_reg = GCC_UFS_TX_SYMBOL_CLK_CORE_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_tx_symbol_clk_core_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_ufs_tx_symbol_clk_core_clk.c),
	},
};

static struct gate_clk gcc_usb3_phy_pipe_clk = {
	.en_reg = GCC_USB3_PHY_PIPE_CBCR,
	.en_mask = BIT(0),
	.delay_us = 50,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_phy_pipe_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_usb3_phy_pipe_clk.c),
	},
};

static struct branch_clk gcc_usb20_master_clk = {
	.cbcr_reg = GCC_USB20_MASTER_CBCR,
	.bcr_reg = GCC_USB_20_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb20_master_clk",
		.parent = &usb20_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb20_master_clk.c),
	},
};

static struct branch_clk gcc_usb20_mock_utmi_clk = {
	.cbcr_reg = GCC_USB20_MOCK_UTMI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb20_mock_utmi_clk",
		.parent = &usb20_mock_utmi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb20_mock_utmi_clk.c),
	},
};

static struct branch_clk gcc_usb20_sleep_clk = {
	.cbcr_reg = GCC_USB20_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb20_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb20_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = GCC_USB30_MASTER_CBCR,
	.bcr_reg = GCC_USB_30_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_master_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_master_clk.c),
	},
};

static struct branch_clk gcc_usb30_mock_utmi_clk = {
	.cbcr_reg = GCC_USB30_MOCK_UTMI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_mock_utmi_clk",
		.parent = &usb30_mock_utmi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_mock_utmi_clk.c),
	},
};

static struct branch_clk gcc_usb30_sleep_clk = {
	.cbcr_reg = GCC_USB30_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb3_phy_aux_clk = {
	.cbcr_reg = GCC_USB3_PHY_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_phy_aux_clk",
		.parent = &usb3_phy_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_phy_aux_clk.c),
	},
};

static struct branch_clk gcc_usb_phy_cfg_ahb2phy_clk = {
	.cbcr_reg = GCC_USB_PHY_CFG_AHB2PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb_phy_cfg_ahb2phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_phy_cfg_ahb2phy_clk.c),
	},
};

static struct branch_clk gcc_hmss_rbcpr_clk = {
	.cbcr_reg = GCC_HMSS_RBCPR_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_hmss_rbcpr_clk",
		.parent = &hmss_rbcpr_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_hmss_rbcpr_clk.c),
	},
};

static struct branch_clk hlos1_vote_lpass_core_smmu_clk = {
	.cbcr_reg = GCC_HLOS1_VOTE_LPASS_CORE_SMMU_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "hlos1_vote_lpass_core_smmu_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hlos1_vote_lpass_core_smmu_clk.c),
	},
};

static struct branch_clk hlos1_vote_lpass_adsp_smmu_clk = {
	.cbcr_reg = GCC_HLOS1_VOTE_LPASS_ADSP_SMMU_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "hlos1_vote_lpass_adsp_smmu_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hlos1_vote_lpass_adsp_smmu_clk.c),
	},
};

static struct reset_clk gcc_usb3_phy_reset = {
	.reset_reg = GCC_USB3_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb3_phy_reset.c),
	},
};

static struct reset_clk gcc_usb3phy_phy_reset = {
	.reset_reg = GCC_USB3PHY_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3phy_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb3phy_phy_reset.c),
	},
};

static struct branch_clk gcc_usb3_clkref_clk = {
	.cbcr_reg = GCC_USB3_CLKREF_EN,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_clkref_clk.c),
	},
};

static struct branch_clk gcc_hdmi_clkref_clk = {
	.cbcr_reg = GCC_HDMI_CLKREF_EN,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_hdmi_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_hdmi_clkref_clk.c),
	},
};

static struct branch_clk gcc_edp_clkref_clk = {
	.cbcr_reg = GCC_EDP_CLKREF_EN,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_edp_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_edp_clkref_clk.c),
	},
};

static struct branch_clk gcc_ufs_clkref_clk = {
	.cbcr_reg = GCC_UFS_CLKREF_EN,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_clkref_clk.c),
	},
};

static struct branch_clk gcc_pcie_clkref_clk = {
	.cbcr_reg = GCC_PCIE_CLKREF_EN,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_clkref_clk.c),
	},
};

static struct local_vote_clk gcc_rx2_usb2_clkref_clk = {
	.cbcr_reg = GCC_RX2_USB2_CLKREF_EN,
	.vote_reg = GCC_RX2_USB2_CLKREF_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_rx2_usb2_clkref_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_rx2_usb2_clkref_clk.c),
	},
};

static struct local_vote_clk gcc_rx1_usb2_clkref_clk = {
	.cbcr_reg = GCC_RX1_USB2_CLKREF_EN,
	.vote_reg = GCC_RX1_USB2_CLKREF_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_rx1_usb2_clkref_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_rx1_usb2_clkref_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_usb3_axi_clk = {
	.cbcr_reg = GCC_SYS_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_ufs_axi_clk = {
	.cbcr_reg = GCC_SYS_NOC_UFS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sys_noc_ufs_axi_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_aggre2_usb3_axi_clk = {
	.cbcr_reg = GCC_AGGRE2_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre2_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_aggre2_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_aggre2_ufs_axi_clk = {
	.cbcr_reg = GCC_AGGRE2_UFS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre2_ufs_axi_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_aggre2_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_mmss_bimc_gfx_clk = {
	.cbcr_reg = GCC_MMSS_BIMC_GFX_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.no_halt_check_on_disable = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mmss_bimc_gfx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_bimc_gfx_clk.c),
	},
};

static struct branch_clk gcc_bimc_gfx_clk = {
	.cbcr_reg = GCC_BIMC_GFX_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_bimc_gfx_clk",
		.always_on = true,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bimc_gfx_clk.c),
	},
};

static struct branch_clk gcc_mss_cfg_ahb_clk = {
	.cbcr_reg = GCC_MSS_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_mss_q6_bimc_axi_clk = {
	.cbcr_reg = GCC_MSS_Q6_BIMC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_q6_bimc_axi_clk",
		.always_on = true,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_q6_bimc_axi_clk.c),
	},
};

static struct gate_clk gpll0_out_msscc = {
	.en_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE_1,
	.en_mask = BIT(2),
	.delay_us = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gpll0_out_msscc",
		.ops = &clk_ops_gate,
		CLK_INIT(gpll0_out_msscc.c),
	},
};

static struct branch_clk gcc_mss_snoc_axi_clk = {
	.cbcr_reg = GCC_MSS_SNOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_snoc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_snoc_axi_clk.c),
	},
};

static struct branch_clk gcc_mss_mnoc_bimc_axi_clk = {
	.cbcr_reg = GCC_MSS_MNOC_BIMC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_mnoc_bimc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_mnoc_bimc_axi_clk.c),
	},
};

static struct branch_clk gcc_dcc_ahb_clk = {
	.cbcr_reg = GCC_DCC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_dcc_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_dcc_ahb_clk.c),
	},
};

static struct branch_clk gcc_aggre0_noc_mpu_cfg_ahb_clk = {
	.cbcr_reg = GCC_AGGRE0_NOC_MPU_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre0_noc_mpu_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_aggre0_noc_mpu_cfg_ahb_clk.c),
	},
};

static const struct msm_reset_map gcc_msm8996_resets[] = {
	[QUSB2PHY_PRIM_BCR] = { 0x12038 },
	[QUSB2PHY_SEC_BCR] = { 0x1203c },
	[BLSP1_BCR] = { 0x17000 },
	[BLSP2_BCR] = { 0x25000 },
	[BOOT_ROM_BCR] = { 0x38000 },
	[PRNG_BCR] = { 0x34000 },
	[UFS_BCR] = { 0x75000 },
	[USB_20_BCR] = { 0x12000 },
	[USB_30_BCR] = { 0x0f000 },
	[USB3_PHY_BCR] = { 0x50020 },
	[USB3PHY_PHY_BCR] = { 0x50024 },
	[PCIE_0_PHY_BCR] = { 0x6c01c },
	[PCIE_1_PHY_BCR] = { 0x6d038 },
	[PCIE_2_PHY_BCR] = { 0x6e038 },
	[PCIE_PHY_BCR] = { 0x6f000 },
	[PCIE_PHY_NOCSR_COM_PHY_BCR] = { 0x6f00C },
	[PCIE_PHY_COM_BCR] = { 0x6f014 },
};

static struct mux_clk gcc_debug_mux;
static struct mux_clk gcc_debug_mux_v2;
static struct clk_ops clk_ops_debug_mux;

static struct measure_clk_data debug_mux_priv = {
	.cxo = &cxo_clk_src.c,
	.plltest_reg = PLLTEST_PAD_CFG,
	.plltest_val = 0x51A00,
	.xo_div4_cbcr = GCC_XO_DIV4_CBCR,
	.ctl_reg = CLOCK_FRQ_MEASURE_CTL,
	.status_reg = CLOCK_FRQ_MEASURE_STATUS,
	.base = &virt_base,
};

static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.base = &virt_dbgbase,
	MUX_REC_SRC_LIST(
		&mmss_gcc_dbg_clk.c,
		&gpu_gcc_dbg_clk.c,
		&gcc_debug_mux_v2.c,
		&cpu_dbg_clk.c,
	),
	MUX_SRC_LIST(
		{ &mmss_gcc_dbg_clk.c, 0x001b },
		{ &gpu_gcc_dbg_clk.c, 0x001b },
		{ &gcc_debug_mux_v2.c, 0xffff },
		{ &cpu_dbg_clk.c, 0x00bb },
		{ &cnoc_clk.c, 0x000e },
		{ &pnoc_clk.c, 0x0011 },
		{ &snoc_clk.c, 0x0000 },
		{ &bimc_clk.c, 0x00ad },
		{ &ce1_clk.c, 0x0099 },
		{ &gcc_ce1_axi_m_clk.c, 0x009a },
		{ &gcc_ce1_ahb_m_clk.c, 0x009b },
		{ &measure_only_bimc_hmss_axi_clk.c, 0x00a5 },
		{ &gcc_periph_noc_usb20_ahb_clk.c, 0x0014 },
		{ &gcc_mmss_noc_cfg_ahb_clk.c, 0x0019 },
		{ &gcc_mmss_bimc_gfx_clk.c, 0x001c},
		{ &gcc_bimc_gfx_clk.c, 0x00af},
		{ &gcc_sys_noc_usb3_axi_clk.c, 0x0006 },
		{ &gcc_sys_noc_ufs_axi_clk.c, 0x0007 },
		{ &gcc_usb30_master_clk.c, 0x002d },
		{ &gcc_usb30_sleep_clk.c, 0x002e },
		{ &gcc_usb30_mock_utmi_clk.c, 0x002f },
		{ &gcc_usb3_phy_aux_clk.c, 0x0030 },
		{ &gcc_usb3_phy_pipe_clk.c, 0x0031 },
		{ &gcc_usb20_master_clk.c, 0x0035 },
		{ &gcc_usb20_sleep_clk.c, 0x0036 },
		{ &gcc_usb20_mock_utmi_clk.c, 0x0037 },
		{ &gcc_usb_phy_cfg_ahb2phy_clk.c, 0x0038 },
		{ &gcc_sdcc1_apps_clk.c, 0x0039 },
		{ &gcc_sdcc1_ahb_clk.c, 0x003a },
		{ &gcc_sdcc2_apps_clk.c, 0x003b },
		{ &gcc_sdcc2_ahb_clk.c, 0x003c },
		{ &gcc_sdcc3_apps_clk.c, 0x003d },
		{ &gcc_sdcc3_ahb_clk.c, 0x003e },
		{ &gcc_sdcc4_apps_clk.c, 0x003f },
		{ &gcc_sdcc4_ahb_clk.c, 0x0040 },
		{ &gcc_blsp1_ahb_clk.c, 0x0041 },
		{ &gcc_blsp1_qup1_spi_apps_clk.c, 0x0043 },
		{ &gcc_blsp1_qup1_i2c_apps_clk.c, 0x0044 },
		{ &gcc_blsp1_uart1_apps_clk.c, 0x0045 },
		{ &gcc_blsp1_qup2_spi_apps_clk.c, 0x0047 },
		{ &gcc_blsp1_qup2_i2c_apps_clk.c, 0x0048 },
		{ &gcc_blsp1_uart2_apps_clk.c, 0x0049 },
		{ &gcc_blsp1_qup3_spi_apps_clk.c, 0x004b },
		{ &gcc_blsp1_qup3_i2c_apps_clk.c, 0x004c },
		{ &gcc_blsp1_uart3_apps_clk.c, 0x004d },
		{ &gcc_blsp1_qup4_spi_apps_clk.c, 0x004f },
		{ &gcc_blsp1_qup4_i2c_apps_clk.c, 0x0050 },
		{ &gcc_blsp1_uart4_apps_clk.c, 0x0051 },
		{ &gcc_blsp1_qup5_spi_apps_clk.c, 0x0053 },
		{ &gcc_blsp1_qup5_i2c_apps_clk.c, 0x0054 },
		{ &gcc_blsp1_uart5_apps_clk.c, 0x0055 },
		{ &gcc_blsp1_qup6_spi_apps_clk.c, 0x0057 },
		{ &gcc_blsp1_qup6_i2c_apps_clk.c, 0x0058 },
		{ &gcc_blsp1_uart6_apps_clk.c, 0x0059 },
		{ &gcc_blsp2_ahb_clk.c, 0x005b },
		{ &gcc_blsp2_qup1_spi_apps_clk.c, 0x005d },
		{ &gcc_blsp2_qup1_i2c_apps_clk.c, 0x005e },
		{ &gcc_blsp2_uart1_apps_clk.c, 0x005f },
		{ &gcc_blsp2_qup2_spi_apps_clk.c, 0x0061 },
		{ &gcc_blsp2_qup2_i2c_apps_clk.c, 0x0062 },
		{ &gcc_blsp2_uart2_apps_clk.c, 0x0063 },
		{ &gcc_blsp2_qup3_spi_apps_clk.c, 0x0065 },
		{ &gcc_blsp2_qup3_i2c_apps_clk.c, 0x0066 },
		{ &gcc_blsp2_uart3_apps_clk.c, 0x0067 },
		{ &gcc_blsp2_qup4_spi_apps_clk.c, 0x0069 },
		{ &gcc_blsp2_qup4_i2c_apps_clk.c, 0x006a },
		{ &gcc_blsp2_uart4_apps_clk.c, 0x006b },
		{ &gcc_blsp2_qup5_spi_apps_clk.c, 0x006d },
		{ &gcc_blsp2_qup5_i2c_apps_clk.c, 0x006e },
		{ &gcc_blsp2_uart5_apps_clk.c, 0x006f },
		{ &gcc_blsp2_qup6_spi_apps_clk.c, 0x0071 },
		{ &gcc_blsp2_qup6_i2c_apps_clk.c, 0x0072 },
		{ &gcc_blsp2_uart6_apps_clk.c, 0x0073 },
		{ &gcc_pdm_ahb_clk.c, 0x0076 },
		{ &gcc_pdm2_clk.c, 0x0078 },
		{ &gcc_prng_ahb_clk.c, 0x0079 },
		{ &gcc_tsif_ahb_clk.c, 0x007a },
		{ &gcc_tsif_ref_clk.c, 0x007b },
		{ &gcc_boot_rom_ahb_clk.c, 0x007e },
		{ &gcc_gp1_clk.c, 0x00e3 },
		{ &gcc_gp2_clk.c, 0x00e4 },
		{ &gcc_gp3_clk.c, 0x00e5 },
		{ &gcc_hmss_rbcpr_clk.c, 0x00ba },
		{ &gcc_pcie_0_slv_axi_clk.c, 0x00e6 },
		{ &gcc_pcie_0_mstr_axi_clk.c, 0x00e7 },
		{ &gcc_pcie_0_cfg_ahb_clk.c, 0x00e8 },
		{ &gcc_pcie_0_aux_clk.c, 0x00e9 },
		{ &gcc_pcie_0_pipe_clk.c, 0x00ea },
		{ &gcc_pcie_1_slv_axi_clk.c, 0x00ec },
		{ &gcc_pcie_1_mstr_axi_clk.c, 0x00ed },
		{ &gcc_pcie_1_cfg_ahb_clk.c, 0x00ee },
		{ &gcc_pcie_1_aux_clk.c, 0x00ef },
		{ &gcc_pcie_1_pipe_clk.c, 0x00f0 },
		{ &gcc_pcie_2_slv_axi_clk.c, 0x00f2 },
		{ &gcc_pcie_2_mstr_axi_clk.c, 0x00f3 },
		{ &gcc_pcie_2_cfg_ahb_clk.c, 0x00f4 },
		{ &gcc_pcie_2_aux_clk.c, 0x00f5 },
		{ &gcc_pcie_2_pipe_clk.c, 0x00f6 },
		{ &gcc_pcie_phy_cfg_ahb_clk.c, 0x00f8 },
		{ &gcc_pcie_phy_aux_clk.c, 0x00f9 },
		{ &gcc_ufs_axi_clk.c, 0x00fc },
		{ &gcc_ufs_ahb_clk.c, 0x00fd },
		{ &gcc_ufs_tx_cfg_clk.c, 0x00fe },
		{ &gcc_ufs_rx_cfg_clk.c, 0x00ff },
		{ &gcc_ufs_tx_symbol_0_clk.c, 0x0100 },
		{ &gcc_ufs_rx_symbol_0_clk.c, 0x0101 },
		{ &gcc_ufs_rx_symbol_1_clk.c, 0x0102 },
		{ &gcc_ufs_unipro_core_clk.c, 0x0106 },
		{ &gcc_ufs_ice_core_clk.c, 0x0107 },
		{ &gcc_ufs_sys_clk_core_clk.c, 0x108},
		{ &gcc_ufs_tx_symbol_clk_core_clk.c, 0x0109 },
		{ &gcc_aggre0_snoc_axi_clk.c, 0x0116 },
		{ &gcc_aggre0_cnoc_ahb_clk.c, 0x0117 },
		{ &gcc_smmu_aggre0_axi_clk.c, 0x0119 },
		{ &gcc_smmu_aggre0_ahb_clk.c, 0x011a },
		{ &gcc_aggre0_noc_qosgen_extref_clk.c, 0x011b },
		{ &gcc_aggre2_ufs_axi_clk.c, 0x0126 },
		{ &gcc_aggre2_usb3_axi_clk.c, 0x0127 },
		{ &gcc_dcc_ahb_clk.c, 0x012b },
		{ &gcc_aggre0_noc_mpu_cfg_ahb_clk.c, 0x012c},
		{ &ipa_clk.c, 0x12f },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

static struct mux_clk gcc_debug_mux_v2 = {
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.base = &virt_dbgbase,
	MUX_SRC_LIST(
		{ &gcc_mss_cfg_ahb_clk.c, 0x0133 },
		{ &gcc_mss_mnoc_bimc_axi_clk.c, 0x0134 },
		{ &gcc_mss_snoc_axi_clk.c, 0x0135 },
		{ &gcc_mss_q6_bimc_axi_clk.c, 0x0136 },
	),
	.c = {
		.dbg_name = "gcc_debug_mux_v2",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gcc_debug_mux_v2.c),
	},
};

static struct clk_lookup msm_clocks_rpm_8996[] = {
	CLK_LIST(cxo_clk_src),
	CLK_LIST(pnoc_a_clk),
	CLK_LIST(pnoc_clk),
	CLK_LIST(bimc_a_clk),
	CLK_LIST(bimc_clk),
	CLK_LIST(cnoc_a_clk),
	CLK_LIST(cnoc_clk),
	CLK_LIST(snoc_a_clk),
	CLK_LIST(snoc_clk),
	CLK_LIST(aggre1_noc_clk),
	CLK_LIST(aggre1_noc_a_clk),
	CLK_LIST(aggre2_noc_clk),
	CLK_LIST(aggre2_noc_a_clk),
	CLK_LIST(mmssnoc_axi_rpm_clk),
	CLK_LIST(mmssnoc_axi_rpm_a_clk),
	CLK_LIST(mmssnoc_axi_clk),
	CLK_LIST(mmssnoc_axi_a_clk),
	CLK_LIST(mmssnoc_gds_clk),
	CLK_LIST(bb_clk1),
	CLK_LIST(bb_clk1_ao),
	CLK_LIST(bb_clk1_pin),
	CLK_LIST(bb_clk1_pin_ao),
	CLK_LIST(bb_clk2),
	CLK_LIST(bb_clk2_ao),
	CLK_LIST(bb_clk2_pin),
	CLK_LIST(bb_clk2_pin_ao),
	CLK_LIST(bimc_msmbus_clk),
	CLK_LIST(bimc_msmbus_a_clk),
	CLK_LIST(ce1_a_clk),
	CLK_LIST(cnoc_msmbus_clk),
	CLK_LIST(cnoc_msmbus_a_clk),
	CLK_LIST(cxo_clk_src_ao),
	CLK_LIST(cxo_dwc3_clk),
	CLK_LIST(cxo_lpm_clk),
	CLK_LIST(cxo_otg_clk),
	CLK_LIST(cxo_pil_lpass_clk),
	CLK_LIST(cxo_pil_ssc_clk),
	CLK_LIST(div_clk1),
	CLK_LIST(div_clk1_ao),
	CLK_LIST(div_clk2),
	CLK_LIST(div_clk2_ao),
	CLK_LIST(div_clk3),
	CLK_LIST(div_clk3_ao),
	CLK_LIST(ipa_a_clk),
	CLK_LIST(ipa_clk),
	CLK_LIST(ln_bb_clk),
	CLK_LIST(ln_bb_a_clk),
	CLK_LIST(ln_bb_clk_pin),
	CLK_LIST(ln_bb_a_clk_pin),
	CLK_LIST(mcd_ce1_clk),
	CLK_LIST(pnoc_keepalive_a_clk),
	CLK_LIST(pnoc_msmbus_clk),
	CLK_LIST(pnoc_msmbus_a_clk),
	CLK_LIST(pnoc_pm_clk),
	CLK_LIST(pnoc_sps_clk),
	CLK_LIST(qcedev_ce1_clk),
	CLK_LIST(qcrypto_ce1_clk),
	CLK_LIST(qdss_a_clk),
	CLK_LIST(qdss_clk),
	CLK_LIST(qseecom_ce1_clk),
	CLK_LIST(rf_clk1),
	CLK_LIST(rf_clk1_ao),
	CLK_LIST(rf_clk1_pin),
	CLK_LIST(rf_clk1_pin_ao),
	CLK_LIST(rf_clk2),
	CLK_LIST(rf_clk2_ao),
	CLK_LIST(rf_clk2_pin),
	CLK_LIST(rf_clk2_pin_ao),
	CLK_LIST(scm_ce1_clk),
	CLK_LIST(snoc_msmbus_clk),
	CLK_LIST(snoc_msmbus_a_clk),
	CLK_LIST(ce1_clk),
	CLK_LIST(gcc_ce1_ahb_m_clk),
	CLK_LIST(gcc_ce1_axi_m_clk),
	CLK_LIST(measure_only_bimc_hmss_axi_clk),
};

static struct clk_lookup msm_clocks_gcc_8996[] = {
	CLK_LIST(gpll0),
	CLK_LIST(gpll0_ao),
	CLK_LIST(gpll0_out_main),
	CLK_LIST(gcc_mmss_gpll0_div_clk),
	CLK_LIST(gpll4),
	CLK_LIST(gpll4_out_main),
	CLK_LIST(ufs_axi_clk_src),
	CLK_LIST(pcie_aux_clk_src),
	CLK_LIST(usb30_master_clk_src),
	CLK_LIST(usb20_master_clk_src),
	CLK_LIST(ufs_ice_core_clk_src),
	CLK_LIST(blsp1_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup1_spi_apps_clk_src),
	CLK_LIST(blsp1_qup2_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup2_spi_apps_clk_src),
	CLK_LIST(blsp1_qup3_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup3_spi_apps_clk_src),
	CLK_LIST(blsp1_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup4_spi_apps_clk_src),
	CLK_LIST(blsp1_qup5_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup5_spi_apps_clk_src),
	CLK_LIST(blsp1_qup6_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup6_spi_apps_clk_src),
	CLK_LIST(blsp1_uart1_apps_clk_src),
	CLK_LIST(blsp1_uart2_apps_clk_src),
	CLK_LIST(blsp1_uart3_apps_clk_src),
	CLK_LIST(blsp1_uart4_apps_clk_src),
	CLK_LIST(blsp1_uart5_apps_clk_src),
	CLK_LIST(blsp1_uart6_apps_clk_src),
	CLK_LIST(blsp2_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup1_spi_apps_clk_src),
	CLK_LIST(blsp2_qup2_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup2_spi_apps_clk_src),
	CLK_LIST(blsp2_qup3_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup3_spi_apps_clk_src),
	CLK_LIST(blsp2_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup4_spi_apps_clk_src),
	CLK_LIST(blsp2_qup5_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup5_spi_apps_clk_src),
	CLK_LIST(blsp2_qup6_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup6_spi_apps_clk_src),
	CLK_LIST(blsp2_uart1_apps_clk_src),
	CLK_LIST(blsp2_uart2_apps_clk_src),
	CLK_LIST(blsp2_uart3_apps_clk_src),
	CLK_LIST(blsp2_uart4_apps_clk_src),
	CLK_LIST(blsp2_uart5_apps_clk_src),
	CLK_LIST(blsp2_uart6_apps_clk_src),
	CLK_LIST(gp1_clk_src),
	CLK_LIST(gp2_clk_src),
	CLK_LIST(gp3_clk_src),
	CLK_LIST(hmss_rbcpr_clk_src),
	CLK_LIST(pdm2_clk_src),
	CLK_LIST(sdcc1_apps_clk_src),
	CLK_LIST(sdcc2_apps_clk_src),
	CLK_LIST(sdcc3_apps_clk_src),
	CLK_LIST(sdcc4_apps_clk_src),
	CLK_LIST(tsif_ref_clk_src),
	CLK_LIST(usb20_mock_utmi_clk_src),
	CLK_LIST(usb30_mock_utmi_clk_src),
	CLK_LIST(usb3_phy_aux_clk_src),
	CLK_LIST(gcc_qusb2phy_prim_reset),
	CLK_LIST(gcc_qusb2phy_sec_reset),
	CLK_LIST(gcc_periph_noc_usb20_ahb_clk),
	CLK_LIST(gcc_aggre0_cnoc_ahb_clk),
	CLK_LIST(gcc_aggre0_snoc_axi_clk),
	CLK_LIST(gcc_smmu_aggre0_ahb_clk),
	CLK_LIST(gcc_smmu_aggre0_axi_clk),
	CLK_LIST(gcc_aggre0_noc_qosgen_extref_clk),
	CLK_LIST(gcc_aggre2_usb3_axi_clk),
	CLK_LIST(gcc_aggre2_ufs_axi_clk),
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_spi_apps_clk),
	CLK_LIST(gcc_blsp1_uart1_apps_clk),
	CLK_LIST(gcc_blsp1_uart2_apps_clk),
	CLK_LIST(gcc_blsp1_uart3_apps_clk),
	CLK_LIST(gcc_blsp1_uart4_apps_clk),
	CLK_LIST(gcc_blsp1_uart5_apps_clk),
	CLK_LIST(gcc_blsp1_uart6_apps_clk),
	CLK_LIST(gcc_blsp2_ahb_clk),
	CLK_LIST(gcc_blsp2_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup5_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup5_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup6_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup6_spi_apps_clk),
	CLK_LIST(gcc_blsp2_uart1_apps_clk),
	CLK_LIST(gcc_blsp2_uart2_apps_clk),
	CLK_LIST(gcc_blsp2_uart3_apps_clk),
	CLK_LIST(gcc_blsp2_uart4_apps_clk),
	CLK_LIST(gcc_blsp2_uart5_apps_clk),
	CLK_LIST(gcc_blsp2_uart6_apps_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gcc_gp1_clk),
	CLK_LIST(gcc_gp2_clk),
	CLK_LIST(gcc_gp3_clk),
	CLK_LIST(gcc_hmss_rbcpr_clk),
	CLK_LIST(gcc_mmss_noc_cfg_ahb_clk),
	CLK_LIST(gcc_sys_noc_usb3_axi_clk),
	CLK_LIST(gcc_sys_noc_ufs_axi_clk),
	CLK_LIST(gcc_pcie_0_phy_reset),
	CLK_LIST(gcc_pcie_0_pipe_clk),
	CLK_LIST(gcc_pcie_0_aux_clk),
	CLK_LIST(gcc_pcie_0_cfg_ahb_clk),
	CLK_LIST(gcc_pcie_0_mstr_axi_clk),
	CLK_LIST(gcc_pcie_0_slv_axi_clk),
	CLK_LIST(gcc_pcie_1_phy_reset),
	CLK_LIST(gcc_pcie_1_pipe_clk),
	CLK_LIST(gcc_pcie_1_aux_clk),
	CLK_LIST(gcc_pcie_1_cfg_ahb_clk),
	CLK_LIST(gcc_pcie_1_mstr_axi_clk),
	CLK_LIST(gcc_pcie_1_slv_axi_clk),
	CLK_LIST(gcc_pcie_2_phy_reset),
	CLK_LIST(gcc_pcie_2_pipe_clk),
	CLK_LIST(gcc_pcie_2_aux_clk),
	CLK_LIST(gcc_pcie_2_cfg_ahb_clk),
	CLK_LIST(gcc_pcie_2_mstr_axi_clk),
	CLK_LIST(gcc_pcie_2_slv_axi_clk),
	CLK_LIST(gcc_pcie_phy_reset),
	CLK_LIST(gcc_pcie_phy_com_reset),
	CLK_LIST(gcc_pcie_phy_nocsr_com_phy_reset),
	CLK_LIST(gcc_pcie_phy_aux_clk),
	CLK_LIST(gcc_pcie_phy_cfg_ahb_clk),
	CLK_LIST(gcc_pdm2_clk),
	CLK_LIST(gcc_pdm_ahb_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(gcc_sdcc1_ahb_clk),
	CLK_LIST(gcc_sdcc1_apps_clk),
	CLK_LIST(gcc_sdcc2_ahb_clk),
	CLK_LIST(gcc_sdcc2_apps_clk),
	CLK_LIST(gcc_sdcc3_ahb_clk),
	CLK_LIST(gcc_sdcc3_apps_clk),
	CLK_LIST(gcc_sdcc4_ahb_clk),
	CLK_LIST(gcc_sdcc4_apps_clk),
	CLK_LIST(gcc_tsif_ahb_clk),
	CLK_LIST(gcc_tsif_ref_clk),
	CLK_LIST(gcc_ufs_ahb_clk),
	CLK_LIST(gcc_ufs_axi_clk),
	CLK_LIST(gcc_ufs_ice_core_clk),
	CLK_LIST(gcc_ufs_rx_symbol_1_clk),
	CLK_LIST(gcc_ufs_unipro_core_clk),
	CLK_LIST(gcc_usb3_phy_pipe_clk),
	CLK_LIST(gcc_usb20_master_clk),
	CLK_LIST(gcc_usb20_mock_utmi_clk),
	CLK_LIST(gcc_usb20_sleep_clk),
	CLK_LIST(gcc_usb30_master_clk),
	CLK_LIST(gcc_usb30_mock_utmi_clk),
	CLK_LIST(gcc_usb30_sleep_clk),
	CLK_LIST(gcc_usb3_phy_aux_clk),
	CLK_LIST(gcc_usb_phy_cfg_ahb2phy_clk),
	CLK_LIST(gcc_ufs_rx_cfg_clk),
	CLK_LIST(gcc_ufs_rx_symbol_0_clk),
	CLK_LIST(gcc_ufs_tx_cfg_clk),
	CLK_LIST(gcc_ufs_tx_symbol_0_clk),
	CLK_LIST(gcc_ufs_sys_clk_core_clk),
	CLK_LIST(gcc_ufs_tx_symbol_clk_core_clk),
	CLK_LIST(hlos1_vote_lpass_core_smmu_clk),
	CLK_LIST(hlos1_vote_lpass_adsp_smmu_clk),
	CLK_LIST(gcc_usb3_phy_reset),
	CLK_LIST(gcc_usb3phy_phy_reset),
	CLK_LIST(gcc_usb3_clkref_clk),
	CLK_LIST(gcc_hdmi_clkref_clk),
	CLK_LIST(gcc_edp_clkref_clk),
	CLK_LIST(gcc_ufs_clkref_clk),
	CLK_LIST(gcc_pcie_clkref_clk),
	CLK_LIST(gcc_rx2_usb2_clkref_clk),
	CLK_LIST(gcc_rx1_usb2_clkref_clk),
	CLK_LIST(gcc_mmss_bimc_gfx_clk),
	CLK_LIST(gcc_bimc_gfx_clk),
	CLK_LIST(gcc_dcc_ahb_clk),
	CLK_LIST(gcc_aggre0_noc_mpu_cfg_ahb_clk),
};

static struct clk_lookup msm_clocks_gcc_8996_v2[] = {
	CLK_LIST(qspi_ser_clk_src),
	CLK_LIST(sdcc1_ice_core_clk_src),
	CLK_LIST(gcc_qspi_ahb_clk),
	CLK_LIST(gcc_qspi_ser_clk),
	CLK_LIST(gcc_sdcc1_ice_core_clk),
	CLK_LIST(gcc_mss_cfg_ahb_clk),
	CLK_LIST(gcc_mss_snoc_axi_clk),
	CLK_LIST(gcc_mss_q6_bimc_axi_clk),
	CLK_LIST(gcc_mss_mnoc_bimc_axi_clk),
	CLK_LIST(gpll0_out_msscc),
};

static void msm_clocks_gcc_8996_v2_fixup(void)
{
	pcie_aux_clk_src.c.fmax[VDD_DIG_LOWER] = 9600000;
	pcie_aux_clk_src.c.fmax[VDD_DIG_LOW] = 19200000;
	pcie_aux_clk_src.c.fmax[VDD_DIG_NOMINAL] = 19200000;
	pcie_aux_clk_src.c.fmax[VDD_DIG_HIGH] = 19200000;

	usb30_master_clk_src.c.fmax[VDD_DIG_LOW] = 133333000;
	usb30_master_clk_src.c.fmax[VDD_DIG_NOMINAL] = 200000000;
	usb30_master_clk_src.c.fmax[VDD_DIG_HIGH] = 240000000;

	sdcc1_apps_clk_src.c.fmax[VDD_DIG_LOW] = 50000000;
	sdcc2_apps_clk_src.c.fmax[VDD_DIG_LOW] = 50000000;
	sdcc3_apps_clk_src.c.fmax[VDD_DIG_LOW] = 50000000;
}

static int msm_gcc_8996_probe(struct platform_device *pdev)
{
	struct resource *res;
	u32 regval;
	const char *compat = NULL;
	int compatlen = 0;
	bool is_v2 = false;
	int ret;

	ret = vote_bimc(&bimc_clk, INT_MAX);
	if (ret < 0)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get CC base.\n");
		return -EINVAL;
	}

	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map in CC registers.\n");
		return -ENOMEM;
	}

	/* Set the HMSS_AHB_CLK_ENA bit to enable the hmss_ahb_clk */
	regval = readl_relaxed(virt_base + GCC_APCS_CLOCK_BRANCH_ENA_VOTE);
	regval |= BIT(21);
	writel_relaxed(regval, virt_base + GCC_APCS_CLOCK_BRANCH_ENA_VOTE);

	vdd_dig.vdd_uv[1] = RPM_REGULATOR_CORNER_SVS_KRAIT;
	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_dig regulator!");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	bimc_clk.c.parent = &cxo_clk_src.c;
	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_rpm_8996,
				    ARRAY_SIZE(msm_clocks_rpm_8996));
	if (ret)
		return ret;

	ret = enable_rpm_scaling();
	if (ret < 0)
		return ret;

	/* Perform revision specific fixes */
	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;
	is_v2 = !strcmp(compat, "qcom,gcc-8996-v2") ||
				!strcmp(compat, "qcom,gcc-8996-v3");
	if (is_v2)
		msm_clocks_gcc_8996_v2_fixup();

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_gcc_8996,
				    ARRAY_SIZE(msm_clocks_gcc_8996));
	if (ret)
		return ret;

	/* Register v2 specific clocks */
	if (is_v2) {
		ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_gcc_8996_v2,
				ARRAY_SIZE(msm_clocks_gcc_8996_v2));
		if (ret)
			return ret;
	}

	/*
	 * Hold an active set vote for the PNOC AHB source. Sleep set vote is 0.
	 */
	clk_set_rate(&pnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pnoc_keepalive_a_clk.c);

	/* This clock is used for all MMSS register access */
	clk_prepare_enable(&gcc_mmss_noc_cfg_ahb_clk.c);

	/* Keep an active vote on CXO in case no other driver votes for it */
	clk_prepare_enable(&cxo_clk_src_ao.c);

	/*
	 * Keep the core memory settings enabled at all times for
	 * gcc_mmss_bimc_gfx_clk.
	 */
	clk_set_flags(&gcc_mmss_bimc_gfx_clk.c, CLKFLAG_RETAIN_MEM);

	/* Register block resets */
	msm_reset_controller_register(pdev, gcc_msm8996_resets,
			ARRAY_SIZE(gcc_msm8996_resets), virt_base);

	dev_info(&pdev->dev, "Registered GCC clocks.\n");
	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-8996" },
	{ .compatible = "qcom,gcc-8996-v2" },
	{ .compatible = "qcom,gcc-8996-v3" },
	{}
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_8996_probe,
	.driver = {
		.name = "qcom,gcc-8996",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_gcc_8996_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}
arch_initcall(msm_gcc_8996_init);

/* ======== Clock Debug Controller ======== */
static struct clk_lookup msm_clocks_measure_8996[] = {
	CLK_LIST(mmss_gcc_dbg_clk),
	CLK_LIST(gpu_gcc_dbg_clk),
	CLK_LIST(cpu_dbg_clk),
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
};

static struct clk_lookup msm_clocks_measure_8996_v2[] = {
	CLK_LIST(mmss_gcc_dbg_clk),
	CLK_LIST(gpu_gcc_dbg_clk),
	CLK_LIST(gcc_debug_mux_v2),
	CLK_LIST(cpu_dbg_clk),
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
};

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-8996" },
	{ .compatible = "qcom,cc-debug-8996-v2" },
	{ .compatible = "qcom,cc-debug-8996-v3" },
	{}
};

static int msm_clock_debug_8996_probe(struct platform_device *pdev)
{
	const char *compat = NULL;
	int compatlen = 0;
	struct resource *res;
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get CC base.\n");
		return -EINVAL;
	}
	virt_dbgbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_dbgbase) {
		dev_err(&pdev->dev, "Failed to map in CC registers.\n");
		return -ENOMEM;
	}

	mmss_gcc_dbg_clk.dev = &pdev->dev;
	mmss_gcc_dbg_clk.clk_id = "debug_mmss_clk";
	cpu_dbg_clk.dev = &pdev->dev;
	cpu_dbg_clk.clk_id = "debug_cpu_clk";

	gpu_gcc_dbg_clk.dev = &pdev->dev;
	gpu_gcc_dbg_clk.clk_id = "debug_gpu_clk";

	/* Perform revision specific fixes */
	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,cc-debug-8996"))
		ret = of_msm_clock_register(pdev->dev.of_node,
				    msm_clocks_measure_8996,
				    ARRAY_SIZE(msm_clocks_measure_8996));
	else
		ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_measure_8996_v2,
				ARRAY_SIZE(msm_clocks_measure_8996_v2));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered debug mux.\n");
	return ret;
}

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_8996_probe,
	.driver = {
		.name = "qcom,cc-debug-8996",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_clock_debug_8996_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}
late_initcall(msm_clock_debug_8996_init);
