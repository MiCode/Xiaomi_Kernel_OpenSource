/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <dt-bindings/clock/msm-clocks-cobalt.h>
#include <dt-bindings/clock/msm-clocks-hwio-cobalt.h>

#include "vdd-level-cobalt.h"

static void __iomem *virt_base;
static void __iomem *virt_dbgbase;

#define cxo_clk_src_source_val 0
#define cxo_clk_src_ao_source_val 0
#define gpll0_out_main_source_val 1
#define gpll0_ao_source_val 1
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

DEFINE_CLK_RPM_SMD_BRANCH(cxo_clk_src, cxo_clk_src_ao, RPM_MISC_CLK_TYPE,
			  CXO_CLK_SRC_ID, 19200000);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(cnoc_periph_clk, cnoc_periph_a_clk, RPM_BUS_CLK_TYPE,
			CNOC_PERIPH_CLK_ID, NULL);
static DEFINE_CLK_VOTER(cnoc_periph_keepalive_a_clk, &cnoc_periph_a_clk.c,
			LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, &cnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, &cnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_ssc_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_spss_clk, &cxo_clk_src.c);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_clk1_ao, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_clk2_ao, DIV_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk3, div_clk3_ao, DIV_CLK3_ID);
DEFINE_CLK_RPM_SMD(ipa_clk, ipa_a_clk, RPM_IPA_CLK_TYPE,
		   IPA_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(ce1_clk, ce1_a_clk, RPM_CE_CLK_TYPE,
		   CE1_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD_XO_BUFFER(ln_bb_clk1, ln_bb_clk1_ao, LN_BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(ln_bb_clk1_pin, ln_bb_clk1_pin_ao,
				     LN_BB_CLK1_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(ln_bb_clk2, ln_bb_clk2_ao, LN_BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(ln_bb_clk2_pin, ln_bb_clk2_pin_ao,
				     LN_BB_CLK2_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(ln_bb_clk3, ln_bb_clk3_ao, LN_BB_CLK3_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(ln_bb_clk3_pin, ln_bb_clk3_pin_ao,
				     LN_BB_CLK3_PIN_ID);
static DEFINE_CLK_VOTER(mcd_ce1_clk, &ce1_clk.c, 85710000);
DEFINE_CLK_DUMMY(measure_only_bimc_hmss_axi_clk, 0);
DEFINE_CLK_RPM_SMD(mmssnoc_axi_clk, mmssnoc_axi_a_clk,
			RPM_MMAXI_CLK_TYPE, MMSSNOC_AXI_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(aggre1_noc_clk, aggre1_noc_a_clk, RPM_AGGR_CLK_TYPE,
				AGGR1_NOC_ID, NULL);
DEFINE_CLK_RPM_SMD(aggre2_noc_clk, aggre2_noc_a_clk, RPM_AGGR_CLK_TYPE,
				AGGR2_NOC_ID, NULL);
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
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk3, rf_clk3_ao, RF_CLK3_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk3_pin, rf_clk3_pin_ao,
				     RF_CLK3_PIN_ID);
static DEFINE_CLK_VOTER(scm_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
DEFINE_CLK_DUMMY(gcc_ce1_ahb_m_clk, 0);
DEFINE_CLK_DUMMY(gcc_ce1_axi_m_clk, 0);

DEFINE_EXT_CLK(debug_mmss_clk, NULL);
DEFINE_EXT_CLK(gpu_gcc_debug_clk, NULL);
DEFINE_EXT_CLK(gfx_gcc_debug_clk, NULL);
DEFINE_EXT_CLK(debug_cpu_clk, NULL);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0 = {
	.en_reg = (void __iomem *)GCC_APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GCC_GPLL0_MODE,
	.status_mask = BIT(31),
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
	.status_mask = BIT(31),
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
	.cbcr_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE_1,
	.vote_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE_1,
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
	.status_mask = BIT(31),
	.base = &virt_base,
	.c = {
		.rate = 384000000,
		.parent = &cxo_clk_src.c,
		.dbg_name = "gpll4",
		.ops = &clk_ops_pll_vote,
		VDD_DIG_FMAX_MAP3(LOWER, 400000000, LOW, 800000000,
					NOMINAL, 1600000000),
		CLK_INIT(gpll4.c),
	},
};
DEFINE_EXT_CLK(gpll4_out_main, &gpll4.c);

static struct clk_freq_tbl ftbl_hmss_ahb_clk_src[] = {
	F(  19200000, cxo_clk_src_ao,    1,    0,     0),
	F(  37500000, gpll0_out_main,   16,    0,     0),
	F(  75000000, gpll0_out_main,    8,    0,     0),
	F_END
};

static struct rcg_clk hmss_ahb_clk_src = {
	.cmd_rcgr_reg = GCC_HMSS_AHB_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_hmss_ahb_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "hmss_ahb_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 50000000,
					NOMINAL, 100000000),
		CLK_INIT(hmss_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb30_master_clk_src[] = {
	F(  19200000,    cxo_clk_src,    1,    0,     0),
	F( 120000000, gpll0_out_main,    5,    0,     0),
	F( 150000000, gpll0_out_main,    4,    0,     0),
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
		VDD_DIG_FMAX_MAP4(LOWER, 66670000, LOW, 133330000,
				NOMINAL, 200000000, HIGH, 240000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pcie_aux_clk_src[] = {
	F(  19200000,    cxo_clk_src,    1,    0,     0),
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
		VDD_DIG_FMAX_MAP2(LOWER, 9600000, LOW, 19200000),
		CLK_INIT(pcie_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ufs_axi_clk_src[] = {
	F(  50000000, gpll0_out_main,   12,    0,     0),
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
		VDD_DIG_FMAX_MAP4(LOWER, 50000000, LOW, 100000000,
				NOMINAL, 200000000, HIGH, 240000000),
		CLK_INIT(ufs_axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_i2c_apps_clk_src[] = {
	F(  19200000,    cxo_clk_src,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_qup_spi_apps_clk_src[] = {
	F(    960000,    cxo_clk_src,   10,    1,     2),
	F(   4800000,    cxo_clk_src,    4,    0,     0),
	F(   9600000,    cxo_clk_src,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  19200000,    cxo_clk_src,    1,    0,     0),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct clk_freq_tbl ftbl_blsp_uart_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  19200000,    cxo_clk_src,    1,    0,     0),
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
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
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

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
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

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
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

static struct rcg_clk blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup1_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp2_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp2_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_qup_spi_apps_clk_src,
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

static struct rcg_clk blsp2_uart1_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
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

static struct rcg_clk blsp2_uart2_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
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

static struct rcg_clk blsp2_uart3_apps_clk_src = {
	.cmd_rcgr_reg = GCC_BLSP2_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000,  LOW, 31580000,
					NOMINAL, 63160000),
		CLK_INIT(blsp2_uart3_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp_clk_src[] = {
	F(  19200000,    cxo_clk_src,    1,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GCC_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp_clk_src,
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

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GCC_GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp_clk_src,
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

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GCC_GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp_clk_src,
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
	F(  19200000,    cxo_clk_src_ao,    1,    0,     0),
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

static struct clk_freq_tbl ftbl_sdcc2_apps_clk_src[] = {
	F(    144000,    cxo_clk_src,   16,    3,    25),
	F(    400000,    cxo_clk_src,   12,    1,     4),
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

static struct clk_freq_tbl ftbl_sdcc4_apps_clk_src[] = {
	F(    144000,    cxo_clk_src,   16,    3,    25),
	F(    400000,    cxo_clk_src,   12,    1,     4),
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
	F(    105495,    cxo_clk_src,    1,    1,   182),
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

static struct clk_freq_tbl ftbl_ufs_ice_core_clk_src[] = {
	F(     75000000,  gpll0_out_main,    8,    0,   0),
	F(    150000000,  gpll0_out_main,    4,    0,   0),
	F(    300000000,  gpll0_out_main,    2,    0,   0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 300000000),
		CLK_INIT(ufs_ice_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ufs_phy_aux_clk_src[] = {
	F(  19200000,    cxo_clk_src,    1,    0,     0),
	F_END
};

static struct rcg_clk ufs_phy_aux_clk_src = {
	.cmd_rcgr_reg = GCC_UFS_PHY_AUX_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ufs_phy_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "ufs_phy_aux_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(ufs_phy_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ufs_unipro_core_clk_src[] = {
	F(  37500000, gpll0_out_main,   16,    0,     0),
	F(  75000000, gpll0_out_main,    8,    0,     0),
	F( 150000000, gpll0_out_main,    4,    0,     0),
	F_END
};

static struct rcg_clk ufs_unipro_core_clk_src = {
	.cmd_rcgr_reg = GCC_UFS_UNIPRO_CORE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ufs_unipro_core_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "ufs_unipro_core_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 37500000, LOW, 75000000,
					NOMINAL, 150000000),
		CLK_INIT(ufs_unipro_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb30_mock_utmi_clk_src[] = {
	F(  19200000,    cxo_clk_src,    1,    0,     0),
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
	F(   1200000,    cxo_clk_src,   16,    0,     0),
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
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(usb3_phy_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_hmss_gpll0_clk_src[] = {
	F( 300000000,  gpll0_ao,    2,    0,     0),
	F_END
};

static struct rcg_clk hmss_gpll0_clk_src = {
	.cmd_rcgr_reg = GCC_HMSS_GPLL0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_hmss_gpll0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "hmss_gpll0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 600000000),
		CLK_INIT(hmss_gpll0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_qspi_ref_clk_src[] = {
	F(  75000000,  gpll0_out_main,    8,    0,     0),
	F( 150000000,  gpll0_out_main,    4,    0,     0),
	F( 256000000,  gpll4_out_main,  1.5,    0,     0),
	F( 300000000,  gpll0_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk qspi_ref_clk_src = {
	.cmd_rcgr_reg = GCC_QSPI_REF_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_qspi_ref_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "qspi_ref_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 40000000, LOW, 160400000,
							NOMINAL, 320800000),
		CLK_INIT(qspi_ref_clk_src.c),
	},
};

static struct branch_clk gcc_hdmi_clkref_clk = {
	.cbcr_reg = GCC_HDMI_CLKREF_EN,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_hdmi_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_hdmi_clkref_clk.c),
	},
};

static struct branch_clk gcc_pcie_clkref_clk = {
	.cbcr_reg = GCC_PCIE_CLKREF_EN,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_clkref_clk.c),
	},
};

static struct branch_clk gcc_rx1_usb2_clkref_clk = {
	.cbcr_reg = GCC_RX1_USB2_CLKREF_EN,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_rx1_usb2_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_rx1_usb2_clkref_clk.c),
	},
};

static struct branch_clk gcc_ufs_clkref_clk = {
	.cbcr_reg = GCC_UFS_CLKREF_EN,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_clkref_clk.c),
	},
};

static struct branch_clk gcc_usb3_clkref_clk = {
	.cbcr_reg = GCC_USB3_CLKREF_EN,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_clkref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_clkref_clk.c),
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

static struct branch_clk gcc_aggre1_ufs_axi_clk = {
	.cbcr_reg = GCC_AGGRE1_UFS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre1_ufs_axi_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_aggre1_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_aggre1_usb3_axi_clk = {
	.cbcr_reg = GCC_AGGRE1_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_aggre1_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_aggre1_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_bimc_mss_q6_axi_clk = {
	.cbcr_reg = GCC_BIMC_MSS_Q6_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_bimc_mss_q6_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bimc_mss_q6_axi_clk.c),
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

static struct branch_clk gcc_cfg_noc_usb3_axi_clk = {
	.cbcr_reg = GCC_CFG_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_cfg_noc_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_cfg_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_bimc_gfx_clk = {
	.cbcr_reg = GCC_BIMC_GFX_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.no_halt_check_on_disable = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_bimc_gfx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bimc_gfx_clk.c),
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

static struct branch_clk gcc_gpu_bimc_gfx_clk = {
	.cbcr_reg = GCC_GPU_BIMC_GFX_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.no_halt_check_on_disable = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gpu_bimc_gfx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gpu_bimc_gfx_clk.c),
	},
};

static struct branch_clk gcc_gpu_bimc_gfx_src_clk = {
	.cbcr_reg = GCC_GPU_BIMC_GFX_SRC_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gpu_bimc_gfx_src_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gpu_bimc_gfx_src_clk.c),
	},
};

static struct branch_clk gcc_gpu_cfg_ahb_clk = {
	.cbcr_reg = GCC_GPU_CFG_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.no_halt_check_on_disable = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gpu_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gpu_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_gpu_snoc_dvm_gfx_clk = {
	.cbcr_reg = GCC_GPU_SNOC_DVM_GFX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gpu_snoc_dvm_gfx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gpu_snoc_dvm_gfx_clk.c),
	},
};

static struct branch_clk gcc_gpu_iref_clk = {
	.cbcr_reg = GCC_GPU_IREF_EN,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gpu_iref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gpu_iref_clk.c),
	},
};

static struct local_vote_clk gcc_hmss_ahb_clk = {
	.cbcr_reg = GCC_HMSS_AHB_CBCR,
	.vote_reg = GCC_APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(21),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_hmss_ahb_clk",
		.always_on = true,
		.parent = &hmss_ahb_clk_src.c,
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_hmss_ahb_clk.c),
	},
};

static struct branch_clk gcc_hmss_dvm_bus_clk = {
	.cbcr_reg = GCC_HMSS_DVM_BUS_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_hmss_dvm_bus_clk",
		.always_on = true,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_hmss_dvm_bus_clk.c),
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

static struct branch_clk gcc_mmss_sys_noc_axi_clk = {
	.cbcr_reg = GCC_MMSS_SYS_NOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mmss_sys_noc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_sys_noc_axi_clk.c),
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

static struct branch_clk gcc_pcie_0_pipe_clk = {
	.cbcr_reg = GCC_PCIE_0_PIPE_CBCR,
	.bcr_reg = GCC_PCIE_0_PHY_BCR,
	.has_sibling = 1,
	.halt_check = DELAY,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_pipe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_pipe_clk.c),
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
	.cbcr_reg = GCC_UFS_AXI_CBCR,
	.bcr_reg = GCC_UFS_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_axi_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_axi_clk.c),
	},
};

static struct hw_ctl_clk gcc_ufs_axi_hw_ctl_clk = {
	.cbcr_reg = GCC_UFS_AXI_CBCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_axi_hw_ctl_clk",
		.parent = &gcc_ufs_axi_clk.c,
		.ops = &clk_ops_branch_hw_ctl,
		CLK_INIT(gcc_ufs_axi_hw_ctl_clk.c),
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

static struct hw_ctl_clk gcc_ufs_ice_core_hw_ctl_clk = {
	.cbcr_reg = GCC_UFS_ICE_CORE_CBCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_ice_core_hw_ctl_clk",
		.parent = &gcc_ufs_ice_core_clk.c,
		.ops = &clk_ops_branch_hw_ctl,
		CLK_INIT(gcc_ufs_ice_core_hw_ctl_clk.c),
	},
};

static struct branch_clk gcc_ufs_phy_aux_clk = {
	.cbcr_reg = GCC_UFS_PHY_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_phy_aux_clk",
		.parent = &ufs_phy_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_phy_aux_clk.c),
	},
};

static struct hw_ctl_clk gcc_ufs_phy_aux_hw_ctl_clk = {
	.cbcr_reg = GCC_UFS_PHY_AUX_CBCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_phy_aux_hw_ctl_clk",
		.parent = &gcc_ufs_phy_aux_clk.c,
		.ops = &clk_ops_branch_hw_ctl,
		CLK_INIT(gcc_ufs_phy_aux_hw_ctl_clk.c),
	},
};

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

static struct gate_clk gcc_ufs_rx_symbol_1_clk = {
	.en_reg = GCC_UFS_RX_SYMBOL_1_CBCR,
	.en_mask = BIT(0),
	.delay_us = 500,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_rx_symbol_1_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_ufs_rx_symbol_1_clk.c),
	},
};

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

static struct branch_clk gcc_ufs_unipro_core_clk = {
	.cbcr_reg = GCC_UFS_UNIPRO_CORE_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_unipro_core_clk",
		.parent = &ufs_unipro_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_unipro_core_clk.c),
	},
};

static struct hw_ctl_clk gcc_ufs_unipro_core_hw_ctl_clk = {
	.cbcr_reg = GCC_UFS_UNIPRO_CORE_CBCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_unipro_core_hw_ctl_clk",
		.parent = &gcc_ufs_unipro_core_clk.c,
		.ops = &clk_ops_branch_hw_ctl,
		CLK_INIT(gcc_ufs_unipro_core_hw_ctl_clk.c),
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
		.depends = &gcc_cfg_noc_usb3_axi_clk.c,
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

static struct branch_clk gcc_wcss_ahb_s0_clk = {
	.cbcr_reg = GCC_WCSS_AHB_S0_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_wcss_ahb_s0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_wcss_ahb_s0_clk.c),
	},
};

static struct branch_clk gcc_wcss_axi_m_clk = {
	.cbcr_reg = GCC_WCSS_AXI_M_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_wcss_axi_m_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_wcss_axi_m_clk.c),
	},
};

static struct branch_clk gcc_wcss_ecahb_clk = {
	.cbcr_reg = GCC_WCSS_ECAHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_wcss_ecahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_wcss_ecahb_clk.c),
	},
};

static struct branch_clk gcc_wcss_shdreg_ahb_clk = {
	.cbcr_reg = GCC_WCSS_SHDREG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_wcss_shdreg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_wcss_shdreg_ahb_clk.c),
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

static struct branch_clk gcc_qspi_ref_clk = {
	.cbcr_reg = GCC_QSPI_REF_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_qspi_ref_clk",
		.parent = &qspi_ref_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_qspi_ref_clk.c),
	},
};

static struct mux_clk gcc_debug_mux;
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
		&gpu_gcc_debug_clk.c,
		&gfx_gcc_debug_clk.c,
		&debug_mmss_clk.c,
	),
	MUX_SRC_LIST(
		{ &gpu_gcc_debug_clk.c, 0x013d },
		{ &gfx_gcc_debug_clk.c, 0x013d },
		{ &debug_mmss_clk.c, 0x0022 },
		{ &snoc_clk.c, 0x0000 },
		{ &cnoc_clk.c, 0x000e },
		{ &bimc_clk.c, 0x00a9 },
		{ &gcc_mmss_sys_noc_axi_clk.c, 0x001f },
		{ &gcc_mmss_noc_cfg_ahb_clk.c, 0x0020 },
		{ &gcc_usb30_master_clk.c, 0x003e },
		{ &gcc_usb30_sleep_clk.c, 0x003f },
		{ &gcc_usb30_mock_utmi_clk.c, 0x0040 },
		{ &gcc_usb3_phy_aux_clk.c, 0x0041 },
		{ &gcc_usb3_phy_pipe_clk.c, 0x0042 },
		{ &gcc_usb_phy_cfg_ahb2phy_clk.c, 0x0045 },
		{ &gcc_sdcc2_apps_clk.c, 0x0046 },
		{ &gcc_sdcc2_ahb_clk.c, 0x0047 },
		{ &gcc_sdcc4_apps_clk.c, 0x0048 },
		{ &gcc_sdcc4_ahb_clk.c, 0x0049 },
		{ &gcc_blsp1_ahb_clk.c, 0x004a },
		{ &gcc_blsp1_qup1_spi_apps_clk.c, 0x004c },
		{ &gcc_blsp1_qup1_i2c_apps_clk.c, 0x004d },
		{ &gcc_blsp1_uart1_apps_clk.c, 0x004e },
		{ &gcc_blsp1_qup2_spi_apps_clk.c, 0x0050 },
		{ &gcc_blsp1_qup2_i2c_apps_clk.c, 0x0051 },
		{ &gcc_blsp1_uart2_apps_clk.c, 0x0052 },
		{ &gcc_blsp1_qup3_spi_apps_clk.c, 0x0054 },
		{ &gcc_blsp1_qup3_i2c_apps_clk.c, 0x0055 },
		{ &gcc_blsp1_uart3_apps_clk.c, 0x0056 },
		{ &gcc_blsp1_qup4_spi_apps_clk.c, 0x0058 },
		{ &gcc_blsp1_qup4_i2c_apps_clk.c, 0x0059 },
		{ &gcc_blsp1_qup5_spi_apps_clk.c, 0x005a },
		{ &gcc_blsp1_qup5_i2c_apps_clk.c, 0x005b },
		{ &gcc_blsp1_qup6_spi_apps_clk.c, 0x005c },
		{ &gcc_blsp1_qup6_i2c_apps_clk.c, 0x005d },
		{ &gcc_blsp2_ahb_clk.c, 0x005e },
		{ &gcc_blsp2_qup1_spi_apps_clk.c, 0x0060 },
		{ &gcc_blsp2_qup1_i2c_apps_clk.c, 0x0061 },
		{ &gcc_blsp2_uart1_apps_clk.c, 0x0062 },
		{ &gcc_blsp2_qup2_spi_apps_clk.c, 0x0064 },
		{ &gcc_blsp2_qup2_i2c_apps_clk.c, 0x0065 },
		{ &gcc_blsp2_uart2_apps_clk.c, 0x0066 },
		{ &gcc_blsp2_qup3_spi_apps_clk.c, 0x0068 },
		{ &gcc_blsp2_qup3_i2c_apps_clk.c, 0x0069 },
		{ &gcc_blsp2_uart3_apps_clk.c, 0x006a },
		{ &gcc_blsp2_qup4_spi_apps_clk.c, 0x006c },
		{ &gcc_blsp2_qup4_i2c_apps_clk.c, 0x006d },
		{ &gcc_blsp2_qup5_spi_apps_clk.c, 0x006e },
		{ &gcc_blsp2_qup5_i2c_apps_clk.c, 0x006f },
		{ &gcc_blsp2_qup6_spi_apps_clk.c, 0x0070 },
		{ &gcc_blsp2_qup6_i2c_apps_clk.c, 0x0071 },
		{ &gcc_pdm_ahb_clk.c, 0x0072 },
		{ &gcc_pdm2_clk.c, 0x0074},
		{ &gcc_prng_ahb_clk.c, 0x0075 },
		{ &gcc_tsif_ahb_clk.c, 0x0076 },
		{ &gcc_tsif_ref_clk.c, 0x0077 },
		{ &gcc_boot_rom_ahb_clk.c, 0x007a },
		{ &ce1_clk.c, 0x0097 },
		{ &gcc_ce1_axi_m_clk.c, 0x0098 },
		{ &gcc_ce1_ahb_m_clk.c, 0x0099 },
		{ &measure_only_bimc_hmss_axi_clk.c, 0x00bb },
		{ &gcc_bimc_gfx_clk.c, 0x00ac },
		{ &gcc_hmss_ahb_clk.c, 0x00ba },
		{ &gcc_hmss_rbcpr_clk.c, 0x00bc },
		{ &gcc_gp1_clk.c, 0x00df },
		{ &gcc_gp2_clk.c, 0x00e0 },
		{ &gcc_gp3_clk.c, 0x00e1 },
		{ &gcc_pcie_0_slv_axi_clk.c, 0x00e2 },
		{ &gcc_pcie_0_mstr_axi_clk.c, 0x00e3 },
		{ &gcc_pcie_0_cfg_ahb_clk.c, 0x00e4 },
		{ &gcc_pcie_0_aux_clk.c, 0x00e5 },
		{ &gcc_pcie_0_pipe_clk.c, 0x00e6 },
		{ &gcc_pcie_phy_aux_clk.c, 0x00e8 },
		{ &gcc_ufs_axi_clk.c, 0x00ea },
		{ &gcc_ufs_ahb_clk.c, 0x00eb },
		{ &gcc_ufs_tx_symbol_0_clk.c, 0x00ec },
		{ &gcc_ufs_rx_symbol_0_clk.c, 0x00ed },
		{ &gcc_ufs_rx_symbol_1_clk.c, 0x0162 },
		{ &gcc_ufs_unipro_core_clk.c, 0x00f0 },
		{ &gcc_ufs_ice_core_clk.c, 0x00f1 },
		{ &gcc_dcc_ahb_clk.c, 0x0119 },
		{ &ipa_clk.c, 0x011b },
		{ &gcc_mss_cfg_ahb_clk.c, 0x011f },
		{ &gcc_mss_q6_bimc_axi_clk.c, 0x0124 },
		{ &gcc_mss_mnoc_bimc_axi_clk.c, 0x0120 },
		{ &gcc_mss_snoc_axi_clk.c, 0x0123 },
		{ &gcc_gpu_cfg_ahb_clk.c, 0x013b },
		{ &gcc_gpu_bimc_gfx_src_clk.c, 0x013e },
		{ &gcc_gpu_bimc_gfx_clk.c, 0x013f },
		{ &gcc_qspi_ahb_clk.c, 0x0156 },
		{ &gcc_qspi_ref_clk.c, 0x0157 },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_rpm_cobalt[] = {
	CLK_LIST(cxo_clk_src),
	CLK_LIST(bimc_clk),
	CLK_LIST(bimc_a_clk),
	CLK_LIST(cnoc_clk),
	CLK_LIST(cnoc_a_clk),
	CLK_LIST(snoc_clk),
	CLK_LIST(snoc_a_clk),
	CLK_LIST(cnoc_periph_clk),
	CLK_LIST(cnoc_periph_a_clk),
	CLK_LIST(cnoc_periph_keepalive_a_clk),
	CLK_LIST(bimc_msmbus_clk),
	CLK_LIST(bimc_msmbus_a_clk),
	CLK_LIST(ce1_clk),
	CLK_LIST(ce1_a_clk),
	CLK_LIST(cnoc_msmbus_clk),
	CLK_LIST(cnoc_msmbus_a_clk),
	CLK_LIST(cxo_clk_src_ao),
	CLK_LIST(cxo_dwc3_clk),
	CLK_LIST(cxo_lpm_clk),
	CLK_LIST(cxo_otg_clk),
	CLK_LIST(cxo_pil_lpass_clk),
	CLK_LIST(cxo_pil_ssc_clk),
	CLK_LIST(cxo_pil_spss_clk),
	CLK_LIST(div_clk1),
	CLK_LIST(div_clk1_ao),
	CLK_LIST(div_clk2),
	CLK_LIST(div_clk2_ao),
	CLK_LIST(div_clk3),
	CLK_LIST(div_clk3_ao),
	CLK_LIST(ipa_clk),
	CLK_LIST(ipa_a_clk),
	CLK_LIST(ln_bb_clk1),
	CLK_LIST(ln_bb_clk1_ao),
	CLK_LIST(ln_bb_clk1_pin),
	CLK_LIST(ln_bb_clk1_pin_ao),
	CLK_LIST(ln_bb_clk2),
	CLK_LIST(ln_bb_clk2_ao),
	CLK_LIST(ln_bb_clk2_pin),
	CLK_LIST(ln_bb_clk2_pin_ao),
	CLK_LIST(ln_bb_clk3),
	CLK_LIST(ln_bb_clk3_ao),
	CLK_LIST(ln_bb_clk3_pin),
	CLK_LIST(ln_bb_clk3_pin_ao),
	CLK_LIST(mcd_ce1_clk),
	CLK_LIST(measure_only_bimc_hmss_axi_clk),
	CLK_LIST(mmssnoc_axi_clk),
	CLK_LIST(mmssnoc_axi_a_clk),
	CLK_LIST(aggre1_noc_clk),
	CLK_LIST(aggre1_noc_a_clk),
	CLK_LIST(aggre2_noc_clk),
	CLK_LIST(aggre2_noc_a_clk),
	CLK_LIST(qcedev_ce1_clk),
	CLK_LIST(qcrypto_ce1_clk),
	CLK_LIST(qdss_clk),
	CLK_LIST(qdss_a_clk),
	CLK_LIST(qseecom_ce1_clk),
	CLK_LIST(rf_clk1),
	CLK_LIST(rf_clk1_ao),
	CLK_LIST(rf_clk1_pin),
	CLK_LIST(rf_clk1_pin_ao),
	CLK_LIST(rf_clk2),
	CLK_LIST(rf_clk2_ao),
	CLK_LIST(rf_clk2_pin),
	CLK_LIST(rf_clk2_pin_ao),
	CLK_LIST(rf_clk3),
	CLK_LIST(rf_clk3_ao),
	CLK_LIST(rf_clk3_pin),
	CLK_LIST(rf_clk3_pin_ao),
	CLK_LIST(scm_ce1_clk),
	CLK_LIST(snoc_msmbus_clk),
	CLK_LIST(snoc_msmbus_a_clk),
	CLK_LIST(gcc_ce1_ahb_m_clk),
	CLK_LIST(gcc_ce1_axi_m_clk),
};

static struct clk_lookup msm_clocks_gcc_cobalt[] = {
	CLK_LIST(gpll0),
	CLK_LIST(gpll0_ao),
	CLK_LIST(gpll0_out_main),
	CLK_LIST(gcc_mmss_gpll0_div_clk),
	CLK_LIST(gpll4),
	CLK_LIST(gpll4_out_main),
	CLK_LIST(hmss_ahb_clk_src),
	CLK_LIST(usb30_master_clk_src),
	CLK_LIST(pcie_aux_clk_src),
	CLK_LIST(ufs_axi_clk_src),
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
	CLK_LIST(gp1_clk_src),
	CLK_LIST(gp2_clk_src),
	CLK_LIST(gp3_clk_src),
	CLK_LIST(hmss_rbcpr_clk_src),
	CLK_LIST(pdm2_clk_src),
	CLK_LIST(sdcc2_apps_clk_src),
	CLK_LIST(sdcc4_apps_clk_src),
	CLK_LIST(tsif_ref_clk_src),
	CLK_LIST(ufs_ice_core_clk_src),
	CLK_LIST(ufs_phy_aux_clk_src),
	CLK_LIST(ufs_unipro_core_clk_src),
	CLK_LIST(usb30_mock_utmi_clk_src),
	CLK_LIST(usb3_phy_aux_clk_src),
	CLK_LIST(hmss_gpll0_clk_src),
	CLK_LIST(qspi_ref_clk_src),
	CLK_LIST(gcc_usb3_phy_reset),
	CLK_LIST(gcc_usb3phy_phy_reset),
	CLK_LIST(gcc_qusb2phy_prim_reset),
	CLK_LIST(gcc_qusb2phy_sec_reset),
	CLK_LIST(gpll0_out_msscc),
	CLK_LIST(gcc_aggre1_ufs_axi_clk),
	CLK_LIST(gcc_aggre1_usb3_axi_clk),
	CLK_LIST(gcc_bimc_mss_q6_axi_clk),
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
	CLK_LIST(gcc_cfg_noc_usb3_axi_clk),
	CLK_LIST(gcc_bimc_gfx_clk),
	CLK_LIST(gcc_gp1_clk),
	CLK_LIST(gcc_gp2_clk),
	CLK_LIST(gcc_gp3_clk),
	CLK_LIST(gcc_gpu_bimc_gfx_clk),
	CLK_LIST(gcc_gpu_bimc_gfx_src_clk),
	CLK_LIST(gcc_gpu_cfg_ahb_clk),
	CLK_LIST(gcc_gpu_snoc_dvm_gfx_clk),
	CLK_LIST(gcc_gpu_iref_clk),
	CLK_LIST(gcc_hmss_ahb_clk),
	CLK_LIST(gcc_hmss_dvm_bus_clk),
	CLK_LIST(gcc_hmss_rbcpr_clk),
	CLK_LIST(gcc_mmss_noc_cfg_ahb_clk),
	CLK_LIST(gcc_mmss_sys_noc_axi_clk),
	CLK_LIST(gcc_pcie_0_aux_clk),
	CLK_LIST(gcc_pcie_0_cfg_ahb_clk),
	CLK_LIST(gcc_pcie_0_mstr_axi_clk),
	CLK_LIST(gcc_pcie_0_pipe_clk),
	CLK_LIST(gcc_pcie_0_slv_axi_clk),
	CLK_LIST(gcc_pcie_phy_aux_clk),
	CLK_LIST(gcc_pcie_phy_reset),
	CLK_LIST(gcc_pcie_phy_com_reset),
	CLK_LIST(gcc_pcie_phy_nocsr_com_phy_reset),
	CLK_LIST(gcc_pdm2_clk),
	CLK_LIST(gcc_pdm_ahb_clk),
	CLK_LIST(gcc_sdcc2_ahb_clk),
	CLK_LIST(gcc_sdcc2_apps_clk),
	CLK_LIST(gcc_sdcc4_ahb_clk),
	CLK_LIST(gcc_sdcc4_apps_clk),
	CLK_LIST(gcc_tsif_ahb_clk),
	CLK_LIST(gcc_tsif_ref_clk),
	CLK_LIST(gcc_ufs_ahb_clk),
	CLK_LIST(gcc_ufs_axi_clk),
	CLK_LIST(gcc_ufs_axi_hw_ctl_clk),
	CLK_LIST(gcc_ufs_ice_core_clk),
	CLK_LIST(gcc_ufs_ice_core_hw_ctl_clk),
	CLK_LIST(gcc_ufs_phy_aux_clk),
	CLK_LIST(gcc_ufs_phy_aux_hw_ctl_clk),
	CLK_LIST(gcc_ufs_rx_symbol_0_clk),
	CLK_LIST(gcc_ufs_rx_symbol_1_clk),
	CLK_LIST(gcc_ufs_tx_symbol_0_clk),
	CLK_LIST(gcc_ufs_unipro_core_clk),
	CLK_LIST(gcc_ufs_unipro_core_hw_ctl_clk),
	CLK_LIST(gcc_usb30_master_clk),
	CLK_LIST(gcc_usb30_mock_utmi_clk),
	CLK_LIST(gcc_usb30_sleep_clk),
	CLK_LIST(gcc_usb3_phy_aux_clk),
	CLK_LIST(gcc_usb3_phy_pipe_clk),
	CLK_LIST(gcc_usb_phy_cfg_ahb2phy_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gcc_wcss_ahb_s0_clk),
	CLK_LIST(gcc_wcss_axi_m_clk),
	CLK_LIST(gcc_wcss_ecahb_clk),
	CLK_LIST(gcc_wcss_shdreg_ahb_clk),
	CLK_LIST(gcc_mss_cfg_ahb_clk),
	CLK_LIST(gcc_mss_q6_bimc_axi_clk),
	CLK_LIST(gcc_mss_mnoc_bimc_axi_clk),
	CLK_LIST(gcc_mss_snoc_axi_clk),
	CLK_LIST(gcc_hdmi_clkref_clk),
	CLK_LIST(gcc_pcie_clkref_clk),
	CLK_LIST(gcc_rx1_usb2_clkref_clk),
	CLK_LIST(gcc_ufs_clkref_clk),
	CLK_LIST(gcc_usb3_clkref_clk),
	CLK_LIST(gcc_dcc_ahb_clk),
	CLK_LIST(hlos1_vote_lpass_core_smmu_clk),
	CLK_LIST(hlos1_vote_lpass_adsp_smmu_clk),
	CLK_LIST(gcc_qspi_ahb_clk),
	CLK_LIST(gcc_qspi_ref_clk),
};

static void msm_gcc_cobalt_v1_fixup(void)
{
	gcc_ufs_rx_symbol_1_clk.c.ops = &clk_ops_dummy;
	qspi_ref_clk_src.c.ops = &clk_ops_dummy;
	gcc_qspi_ref_clk.c.ops = &clk_ops_dummy;
	gcc_qspi_ahb_clk.c.ops = &clk_ops_dummy;
}

static void msm_gcc_cobalt_v2_fixup(void)
{
	qspi_ref_clk_src.c.ops = &clk_ops_dummy;
	gcc_qspi_ref_clk.c.ops = &clk_ops_dummy;
	gcc_qspi_ahb_clk.c.ops = &clk_ops_dummy;
}

static int msm_gcc_cobalt_probe(struct platform_device *pdev)
{
	struct resource *res;
	u32 regval;
	int ret;
	bool is_v1 = 0, is_v2 = 0;

	ret = vote_bimc(&bimc_clk, INT_MAX);
	if (ret < 0)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get CC base\n");
		return -EINVAL;
	}

	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map in CC registers\n");
		return -ENOMEM;
	}

	/* Set the HMSS_AHB_CLK_ENA bit to enable the hmss_ahb_clk */
	regval = readl_relaxed(virt_base + GCC_APCS_CLOCK_BRANCH_ENA_VOTE);
	regval |= BIT(21);
	writel_relaxed(regval, virt_base + GCC_APCS_CLOCK_BRANCH_ENA_VOTE);

	/*
	 * Set the HMSS_AHB_CLK_SLEEP_ENA bit to allow the hmss_ahb_clk to be
	 * turned off by hardware during certain apps low power modes.
	 */
	regval = readl_relaxed(virt_base + GCC_APCS_CLOCK_SLEEP_ENA_VOTE);
	regval |= BIT(21);
	writel_relaxed(regval, virt_base + GCC_APCS_CLOCK_SLEEP_ENA_VOTE);

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_dig regulator\n");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	bimc_clk.c.parent = &cxo_clk_src.c;
	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_rpm_cobalt,
				    ARRAY_SIZE(msm_clocks_rpm_cobalt));
	if (ret)
		return ret;

	ret = enable_rpm_scaling();
	if (ret < 0)
		return ret;

	is_v1 = of_device_is_compatible(pdev->dev.of_node, "qcom,gcc-cobalt");
	if (is_v1)
		msm_gcc_cobalt_v1_fixup();

	is_v2 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gcc-cobalt-v2");
	if (is_v2)
		msm_gcc_cobalt_v2_fixup();

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_gcc_cobalt,
				    ARRAY_SIZE(msm_clocks_gcc_cobalt));
	if (ret)
		return ret;

	/* Hold an active set vote for the cnoc_periph resource */
	clk_set_rate(&cnoc_periph_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&cnoc_periph_keepalive_a_clk.c);

	/* This clock is used for all MMSSCC register access */
	clk_prepare_enable(&gcc_mmss_noc_cfg_ahb_clk.c);

	/* This clock is used for all GPUCC register access */
	clk_prepare_enable(&gcc_gpu_cfg_ahb_clk.c);

	/* Keep an active vote on CXO in case no other driver votes for it */
	clk_prepare_enable(&cxo_clk_src_ao.c);

	clk_set_flags(&gcc_gpu_bimc_gfx_clk.c, CLKFLAG_RETAIN_MEM);

	dev_info(&pdev->dev, "Registered GCC clocks\n");
	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-cobalt" },
	{ .compatible = "qcom,gcc-cobalt-v2" },
	{ .compatible = "qcom,gcc-hamster" },
	{}
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_cobalt_probe,
	.driver = {
		.name = "qcom,gcc-cobalt",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_gcc_cobalt_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}
arch_initcall(msm_gcc_cobalt_init);

/* ======== Clock Debug Controller ======== */
static struct clk_lookup msm_clocks_measure_cobalt[] = {
	CLK_LIST(gpu_gcc_debug_clk),
	CLK_LIST(gfx_gcc_debug_clk),
	CLK_LIST(debug_mmss_clk),
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
};

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-cobalt" },
	{}
};

static int msm_clock_debug_cobalt_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get CC base\n");
		return -EINVAL;
	}
	virt_dbgbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_dbgbase) {
		dev_err(&pdev->dev, "Failed to map in CC registers\n");
		return -ENOMEM;
	}

	gpu_gcc_debug_clk.dev = &pdev->dev;
	gpu_gcc_debug_clk.clk_id = "debug_gpu_clk";

	gfx_gcc_debug_clk.dev = &pdev->dev;
	gfx_gcc_debug_clk.clk_id = "debug_gfx_clk";

	debug_mmss_clk.dev = &pdev->dev;
	debug_mmss_clk.clk_id = "debug_mmss_clk";

	ret = of_msm_clock_register(pdev->dev.of_node,
				    msm_clocks_measure_cobalt,
				    ARRAY_SIZE(msm_clocks_measure_cobalt));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered debug mux\n");
	return ret;
}

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_cobalt_probe,
	.driver = {
		.name = "qcom,cc-debug-cobalt",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_clock_debug_cobalt_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}
late_initcall(msm_clock_debug_cobalt_init);
