/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/rpm-smd.h>

#include <linux/clk/msm-clock-generic.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/mdm-clocks-9607.h>
#include <dt-bindings/clock/mdm-clocks-hwio-9607.h>

#include "clock.h"

enum {
	GCC_BASE,
	DEBUG_BASE,
	APCS_PLL_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))

DEFINE_CLK_RPM_SMD_BRANCH(xo_clk_src, xo_a_clk_src,
				RPM_MISC_CLK_TYPE, XO_ID, 19200000);

DEFINE_CLK_RPM_SMD(pcnoc_clk, pcnoc_a_clk, RPM_BUS_CLK_TYPE, PCNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_ID, NULL);
DEFINE_CLK_RPM_SMD(qpic_clk, qpic_a_clk, RPM_QPIC_CLK_TYPE, QPIC_ID, NULL);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk1, bb_clk1_a, BB_CLK1_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk1_pin, bb_clk1_a_pin, BB_CLK1_ID);

static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_usb_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_usb_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_keepalive_a_clk, &pcnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_msmbus_clk, &pcnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_msmbus_a_clk, &pcnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_usb_clk, &pcnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_usb_a_clk, &pcnoc_a_clk.c, LONG_MAX);

static DEFINE_CLK_BRANCH_VOTER(xo_lpm_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_otg_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_mss_clk, &xo_clk_src.c);

#define F_APCS_STROMER_PLL(f, l, m, pre_div, post_div, vco) \
	{ \
		.freq_hz = (f), \
		.l_val = (l), \
		.a_val = (m), \
		.pre_div_val = BVAL(14, 12, (pre_div)), \
		.post_div_val = BVAL(11, 8, (post_div)), \
		.vco_val = BVAL(21, 20, (vco)), \
	}

static DEFINE_VDD_REGULATORS(vdd_stromer_pll, VDD_DIG_NUM, 1, vdd_corner, NULL);

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
	.update_mask = BIT(22),
	.post_div_mask = BM(11, 8),
};

static struct alpha_pll_vco_tbl p_vco[] = {
	VCO(0,  700000000, 1400000000),
};

static struct alpha_pll_clk a7sspll = {
	.masks = &pll_masks_p,
	.vco_tbl = p_vco,
	.num_vco = ARRAY_SIZE(p_vco),
	.base = &virt_bases[APCS_PLL_BASE],
	.offset = APCS_MODE,
	.slew = true,
	.enable_config = 1,
	.post_div_config = 0,
	.c = {
		.parent = &xo_a_clk_src.c,
		.dbg_name = "a7sspll",
		.ops = &clk_ops_dyna_alpha_pll,
		VDD_STROMER_FMAX_MAP1(LOW, 1400000000),
		CLK_INIT(a7sspll.c),
	},
};

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.base = &virt_bases[GCC_BASE],
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll0_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_clk_src.c),
	},
};

static struct pll_vote_clk gpll0_ao_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll0_ao_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src.c),
	},
};

static struct pll_vote_clk gpll2_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(3),
	.status_reg = (void __iomem *)GPLL2_MODE,
	.status_mask = BIT(30),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 480000000,
		.dbg_name = "gpll2_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll2_clk_src.c),
	},
};

static struct pll_vote_clk gpll1_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)GPLL1_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 614400000,
		.dbg_name = "gpll1_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_apss_ahb_clk_src[] = {
	F(  19200000,           xo_a,    1,    0,     0),
	F(  50000000,          gpll0,   16,    0,     0),
	F( 100000000,          gpll0,    8,    0,     0),
	F_END
};

static struct rcg_clk apss_ahb_clk_src = {
	.cmd_rcgr_reg = APSS_AHB_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_apss_ahb_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "apss_ahb_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(apss_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_emac_0_125m_clk_src[] = {
	F(      19200000,                   xo,    1,    0,     0),
	F_EXT(  25000000,      emac_0_125m_clk,    5,    0,     0),
	F_EXT( 125000000,      emac_0_125m_clk,    1,    0,     0),
	F_END
};

static struct rcg_clk emac_0_125m_clk_src = {
	.cmd_rcgr_reg = EMAC_0_125M_CMD_RCGR,
	.current_freq = ftbl_emac_0_125m_clk_src,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_emac_0_125m_clk_src,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac_0_125m_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 125000000),
		CLK_INIT(emac_0_125m_clk_src.c),
		},
};

static struct clk_freq_tbl ftbl_emac_0_sys_25m_clk_src[] = {
	F(      19200000,                   xo,    1,    0,     0),
	F_EXT(	25000000,      emac_0_125m_clk,    5,    0,     0),
	F_EXT( 125000000,      emac_0_125m_clk,    1,    0,     0),
	F_END
};
static struct rcg_clk emac_0_sys_25m_clk_src = {
	.cmd_rcgr_reg = EMAC_0_SYS_25M_CMD_RCGR,
	.current_freq = ftbl_emac_0_sys_25m_clk_src,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_emac_0_sys_25m_clk_src,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac_0_sys_25m_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 125000000),
		CLK_INIT(emac_0_sys_25m_clk_src.c),
		},
};

static struct clk_freq_tbl ftbl_emac_0_tx_clk_src[] = {
	F(      19200000,                   xo,    1,    0,     0),
	F_EXT( 125000000,        emac_0_tx_clk,    1,    0,     0),
	F_END
};
static struct rcg_clk emac_0_tx_clk_src = {
	.cmd_rcgr_reg = EMAC_0_TX_CMD_RCGR,
	.current_freq = ftbl_emac_0_tx_clk_src,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_emac_0_tx_clk_src,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac_0_tx_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 125000000),
		CLK_INIT(emac_0_tx_clk_src.c),
		},
};

static struct clk_freq_tbl ftbl_blsp_i2c_apps_clk_src[] = {
	F(  19200000,             xo,    1,    0,     0),
	F(  50000000,          gpll0,   16,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup1_spi_apps_clk_src[] = {
	F(    960000,             xo,   10,    1,     2),
	F(   4800000,             xo,    4,    0,     0),
	F(   9600000,             xo,    2,    0,     0),
	F(  16000000,          gpll0,   10,    1,     5),
	F(  19200000,             xo,    1,    0,     0),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  50000000,          gpll0,   16,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup1_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup2_spi_apps_clk_src[] = {
	F(    960000,             xo,   10,    1,     2),
	F(   4800000,             xo,    4,    0,     0),
	F(   9600000,             xo,    2,    0,     0),
	F(  16000000,          gpll0,   10,    1,     5),
	F(  19200000,             xo,    1,    0,     0),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  50000000,          gpll0,   16,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup2_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup3_spi_apps_clk_src[] = {
	F(    960000,             xo,   10,    1,     2),
	F(   4800000,             xo,    4,    0,     0),
	F(   9600000,             xo,    2,    0,     0),
	F(  16000000,          gpll0,   10,    1,     5),
	F(  18180000,          gpll0,    1,    1,    44),
	F(  19200000,             xo,    1,    0,     0),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  36360000,          gpll0,    1,    1,    22),
	F_END
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup3_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 18180000, NOMINAL, 36360000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup4_spi_apps_clk_src[] = {
	F(    960000,             xo,   10,    1,     2),
	F(   4800000,             xo,    4,    0,     0),
	F(   9600000,             xo,    2,    0,     0),
	F(  16000000,          gpll0,   10,    1,     5),
	F(  19200000,             xo,    1,    0,     0),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  50000000,          gpll0,   16,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup4_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup5_spi_apps_clk_src[] = {
	F(    960000,             xo,   10,    1,     2),
	F(   4800000,             xo,    4,    0,     0),
	F(   9600000,             xo,    2,    0,     0),
	F(  16000000,          gpll0,   10,    1,     5),
	F(  19200000,             xo,    1,    0,     0),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  50000000,          gpll0,   16,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup5_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup6_spi_apps_clk_src[] = {
	F(    960000,             xo,   10,    1,     2),
	F(   4800000,             xo,    4,    0,     0),
	F(   9600000,             xo,    2,    0,     0),
	F(  16000000,          gpll0,   10,    1,     5),
	F(  19200000,             xo,    1,    0,     0),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  50000000,          gpll0,   16,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup6_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_uart_apps_clk_src[] = {
	F(   3686400,          gpll0,    1,   72, 15625),
	F(   7372800,          gpll0,    1,  144, 15625),
	F(  14745600,          gpll0,    1,  288, 15625),
	F(  16000000,          gpll0,   10,    1,     5),
	F(  19200000,             xo,    1,    0,     0),
	F(  24000000,          gpll0,    1,    3,   100),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  32000000,          gpll0,    1,    1,    25),
	F(  40000000,          gpll0,    1,    1,    20),
	F(  46400000,          gpll0,    1,   29,   500),
	F(  48000000,          gpll0,    1,    3,    50),
	F(  51200000,          gpll0,    1,    8,   125),
	F(  56000000,          gpll0,    1,    7,   100),
	F(  58982400,          gpll0,    1, 1152, 15625),
	F(  60000000,          gpll0,    1,    3,    40),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 48480000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 48480000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 48480000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 48480000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart5_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 48480000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart5_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart6_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 48480000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart6_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_crypto_clk_src[] = {
	F(  50000000,          gpll0,   16,    0,     0),
	F(  80000000,          gpll0,   10,    0,     0),
	F( 100000000,          gpll0,    8,    0,     0),
	F( 160000000,          gpll0,    5,    0,     0),
	F_END
};

static struct rcg_clk crypto_clk_src = {
	.cmd_rcgr_reg = CRYPTO_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_crypto_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "crypto_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 80000000, NOMINAL, 160000000),
		CLK_INIT(crypto_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp_clk_src[] = {
	F(  19200000,             xo,    1,    0,     0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GP2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GP3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pdm2_clk_src[] = {
	F(  64000000,          gpll0, 12.5,    0,     0),
	F_END
};

static struct rcg_clk pdm2_clk_src = {
	.cmd_rcgr_reg = PDM2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_pdm2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pdm2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 64000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc_apps_clk[] = {
	F(    144000,             xo,   16,    3,    25),
	F(    400000,             xo,   12,    1,     4),
	F(  20000000,          gpll0,   10,    1,     4),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  50000000,          gpll0,   16,    0,     0),
	F( 100000000,          gpll0,    8,    0,     0),
	F( 177777778,          gpll0,  4.5,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 200000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb_hs_system_clk_src[] = {
	F(  19200000,             xo,    1,    0,     0),
	F(  57140000,          gpll0,   14,    0,     0),
	F(  69565000,          gpll0, 11.5,    0,     0),
	F( 133330000,          gpll0,    6,    0,     0),
	F( 177778000,          gpll0,  4.5,    0,     0),
	F_END
};

static struct rcg_clk usb_hs_system_clk_src = {
	.cmd_rcgr_reg = USB_HS_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb_hs_system_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hs_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 69570000, NOMINAL, 133330000,
						HIGH, 177780000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb_hsic_clk_src[] = {
	F( 480000000,          gpll2,    1,    0,     0),
	F_END
};

static struct rcg_clk usb_hsic_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb_hsic_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 480000000),
		CLK_INIT(usb_hsic_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb_hsic_io_cal_clk_src[] = {
	F(   9600000,             xo,    2,    0,     0),
	F_END
};

static struct rcg_clk usb_hsic_io_cal_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_IO_CAL_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb_hsic_io_cal_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_io_cal_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 9600000),
		CLK_INIT(usb_hsic_io_cal_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb_hsic_system_clk_src[] = {
	F(  19200000,             xo,    1,    0,     0),
	F(  57140000,          gpll0,   14,    0,     0),
	F( 133330000,          gpll0,    6,    0,     0),
	F( 177778000,          gpll0,  4.5,    0,     0),
	F_END
};

static struct rcg_clk usb_hsic_system_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb_hsic_system_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 57140000, NOMINAL, 133330000,
							HIGH, 177778000),
		CLK_INIT(usb_hsic_system_clk_src.c),
	},
};

static struct local_vote_clk gcc_apss_ahb_clk = {
	.cbcr_reg = APSS_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(14),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_apss_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_apss_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_apss_axi_clk = {
	.cbcr_reg = APSS_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(13),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_apss_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_apss_axi_clk.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_boot_rom_ahb_clk = {
	.cbcr_reg = BOOT_ROM_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(7),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_crypto_ahb_clk = {
	.cbcr_reg = CRYPTO_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_crypto_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_crypto_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_crypto_axi_clk = {
	.cbcr_reg = CRYPTO_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(1),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_crypto_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_crypto_axi_clk.c),
	},
};

static struct local_vote_clk gcc_crypto_clk = {
	.cbcr_reg = CRYPTO_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(2),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_crypto_clk",
		.parent = &crypto_clk_src.c,
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_crypto_clk.c),
	},
};

static struct local_vote_clk gcc_apss_tcu_clk = {
	.cbcr_reg = APSS_TCU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(1),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_apss_tcu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_apss_tcu_clk.c),
	},
};

static struct local_vote_clk gcc_prng_ahb_clk = {
	.cbcr_reg = PRNG_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(8),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_prng_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_prng_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_smmu_cfg_clk = {
	.cbcr_reg = SMMU_CFG_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(12),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_smmu_cfg_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_smmu_cfg_clk.c),
	},
};

static struct local_vote_clk gcc_qdss_dap_clk = {
	.cbcr_reg = QDSS_DAP_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(19),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_qdss_dap_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_qdss_dap_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.parent = &blsp1_qup1_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.parent = &blsp1_qup1_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.parent = &blsp1_qup2_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.parent = &blsp1_qup2_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.parent = &blsp1_qup3_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.parent = &blsp1_qup3_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.parent = &blsp1_qup4_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.parent = &blsp1_qup4_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.parent = &blsp1_qup5_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.parent = &blsp1_qup5_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.parent = &blsp1_qup6_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.parent = &blsp1_qup6_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart1_apps_clk = {
	.cbcr_reg = BLSP1_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.parent = &blsp1_uart1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart2_apps_clk = {
	.cbcr_reg = BLSP1_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.parent = &blsp1_uart2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart3_apps_clk = {
	.cbcr_reg = BLSP1_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.parent = &blsp1_uart3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart4_apps_clk = {
	.cbcr_reg = BLSP1_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.parent = &blsp1_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart5_apps_clk = {
	.cbcr_reg = BLSP1_UART5_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.parent = &blsp1_uart5_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart6_apps_clk = {
	.cbcr_reg = BLSP1_UART6_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
		.parent = &blsp1_uart6_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart6_apps_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp1_clk",
		.parent = &gp1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GP2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp2_clk",
		.parent = &gp2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GP3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp3_clk",
		.parent = &gp3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_mss_cfg_ahb_clk = {
	.cbcr_reg = MSS_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mss_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_mss_q6_bimc_axi_clk = {
	.cbcr_reg = MSS_Q6_BIMC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mss_q6_bimc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_q6_bimc_axi_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pdm2_clk",
		.parent = &pdm2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm2_clk.c),
	},
};

static struct branch_clk gcc_pdm_ahb_clk = {
	.cbcr_reg = PDM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pdm_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_ahb_clk = {
	.cbcr_reg = SDCC1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_apps_clk = {
	.cbcr_reg = SDCC1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_apps_clk",
		.parent = &sdcc1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_ahb_clk = {
	.cbcr_reg = SDCC2_AHB_CBCR,
	.has_sibling = 1,
	.toggle_memory = true,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_apps_clk = {
	.cbcr_reg = SDCC2_APPS_CBCR,
	.has_sibling = 0,
	.toggle_memory = true,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc2_apps_clk",
		.parent = &sdcc2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_apps_clk.c),
	},
};

static struct branch_clk gcc_emac_0_125m_clk = {
	.cbcr_reg = EMAC_0_125M_CBCR,
	.has_sibling = 0,
	.toggle_memory = true,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac_0_125m_clk",
		.parent = &emac_0_125m_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac_0_125m_clk.c),
	},
};

static struct branch_clk gcc_emac_0_ahb_clk = {
	.cbcr_reg = EMAC_0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac_0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac_0_ahb_clk.c),
	},
};

static struct branch_clk gcc_emac_0_axi_clk = {
	.cbcr_reg = EMAC_0_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac_0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac_0_axi_clk.c),
	},
};

static struct branch_clk gcc_emac_0_sys_25m_clk = {
	.cbcr_reg = EMAC_0_SYS_25M_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac_0_sys_25m_clk",
		.parent = &emac_0_sys_25m_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac_0_sys_25m_clk.c),
	},
};

static struct branch_clk gcc_emac_0_sys_clk = {
	.cbcr_reg = EMAC_0_SYS_CBCR,
	.has_sibling = 1,
	.toggle_memory = true,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac_0_sys_clk",
		.parent = &emac_0_125m_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac_0_sys_clk.c),
	},
};

static struct branch_clk gcc_emac_0_tx_clk = {
	.cbcr_reg = EMAC_0_TX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac_0_tx_clk",
		.parent = &emac_0_tx_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac_0_tx_clk.c),
	},
};

static struct gate_clk gcc_emac_0_rx_clk = {
	.en_reg = EMAC_0_RX_CBCR,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac_0_rx_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_emac_0_rx_clk.c),
	},
};

static struct branch_clk gcc_usb2a_phy_sleep_clk = {
	.cbcr_reg = USB2A_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2a_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2a_phy_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_phy_cfg_ahb_clk = {
	.cbcr_reg = USB_HS_PHY_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_phy_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_phy_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_ahb_clk = {
	.cbcr_reg = USB_HS_AHB_CBCR,
	.has_sibling = 1,
	.toggle_memory = true,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_system_clk = {
	.cbcr_reg = USB_HS_SYSTEM_CBCR,
	.bcr_reg = USB_HS_BCR,
	.has_sibling = 0,
	.toggle_memory = true,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_system_clk",
		.parent = &usb_hs_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_ahb_clk = {
	.cbcr_reg = USB_HSIC_AHB_CBCR,
	.has_sibling = 1,
	.toggle_memory = true,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_clk = {
	.cbcr_reg = USB_HSIC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_clk",
		.parent = &usb_hsic_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_io_cal_clk = {
	.cbcr_reg = USB_HSIC_IO_CAL_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_io_cal_clk",
		.parent = &usb_hsic_io_cal_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_io_cal_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_io_cal_sleep_clk = {
	.cbcr_reg = USB_HSIC_IO_CAL_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_io_cal_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_io_cal_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_system_clk = {
	.cbcr_reg = USB_HSIC_SYSTEM_CBCR,
	.bcr_reg  = USB_HS_HSIC_BCR,
	.toggle_memory = true,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_system_clk",
		.parent = &usb_hsic_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_system_clk.c),
	},
};

static struct reset_clk gcc_usb2_hs_phy_only_clk = {
	.reset_reg = USB2_HS_PHY_ONLY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2_hs_phy_only_clk",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb2_hs_phy_only_clk.c),
	},
};

static struct reset_clk gcc_qusb2_phy_clk = {
	.reset_reg = QUSB2_PHY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_qusb2_phy_clk",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_qusb2_phy_clk.c),
	},
};

static struct measure_clk apc0_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "apc0_m_clk",
		CLK_INIT(apc0_m_clk.c),
	},
};

static struct measure_clk l2_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "l2_m_clk",
		CLK_INIT(l2_m_clk.c),
	},
};

static void __iomem *meas_base;

static struct mux_clk apss_debug_pri_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x7,
	.shift = 3,
	MUX_SRC_LIST(
		{&apc0_m_clk.c, 3},
		{&l2_m_clk.c, 2},
	),
	.base = &meas_base,
	.c = {
		.dbg_name = "apss_debug_pri_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(apss_debug_pri_mux.c),
	},
};

static int  gcc_set_mux_sel(struct mux_clk *clk, int sel)
{
	u32 regval;

	regval = readl_relaxed(GCC_REG_BASE(GCC_DEBUG_CLK_CTL));
	regval &= 0x1FF;
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	if (sel == 0xFFFF)
		return 0;
	mux_reg_ops.set_mux_sel(clk, sel);

	return 0;
}

static struct measure_clk_data debug_mux_priv = {
	.cxo = &xo_clk_src.c,
	.plltest_reg = PLLTEST_PAD_CFG,
	.plltest_val = 0x51A00,
	.xo_div4_cbcr = GCC_XO_DIV4_CBCR,
	.ctl_reg = CLOCK_FRQ_MEASURE_CTL,
	.status_reg = CLOCK_FRQ_MEASURE_STATUS,
	.base = &virt_bases[DEBUG_BASE],
};
static struct clk_ops clk_ops_debug_mux;
static struct clk_mux_ops gcc_debug_mux_ops;
static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &gcc_debug_mux_ops,
	.offset = GCC_DEBUG_CLK_CTL,
	.mask = 0x1FF,
	.en_offset = GCC_DEBUG_CLK_CTL,
	.en_mask = BIT(16),
	.base = &virt_bases[DEBUG_BASE],
	MUX_SRC_LIST(
		{ &pcnoc_clk.c, 0x0008 },
		{ &bimc_clk.c, 0x155 },
		{ &gcc_gp1_clk.c, 0x0010 },
		{ &gcc_gp2_clk.c, 0x0011 },
		{ &gcc_gp3_clk.c, 0x0012 },
		{ &gcc_mss_cfg_ahb_clk.c, 0x0030 },
		{ &gcc_mss_q6_bimc_axi_clk.c, 0x0031 },
		{ &gcc_qdss_dap_clk.c, 0x0049 },
		{ &gcc_apss_tcu_clk.c, 0x0050 },
		{ &gcc_smmu_cfg_clk.c, 0x005b },
		{ &gcc_usb_hs_system_clk.c, 0x0060 },
		{ &gcc_usb_hs_ahb_clk.c, 0x0061 },
		{ &gcc_usb2a_phy_sleep_clk.c, 0x0063 },
		{ &gcc_usb_hs_phy_cfg_ahb_clk.c, 0x0064 },
		{ &gcc_sdcc1_apps_clk.c, 0x0068 },
		{ &gcc_sdcc1_ahb_clk.c, 0x0069 },
		{ &gcc_sdcc2_apps_clk.c, 0x0070 },
		{ &gcc_sdcc2_ahb_clk.c, 0x0071 },
		{ &gcc_blsp1_ahb_clk.c, 0x0088 },
		{ &gcc_blsp1_qup1_spi_apps_clk.c, 0x008a },
		{ &gcc_blsp1_qup1_i2c_apps_clk.c, 0x008b },
		{ &gcc_blsp1_uart1_apps_clk.c, 0x008c },
		{ &gcc_blsp1_qup2_spi_apps_clk.c, 0x008e },
		{ &gcc_blsp1_qup2_i2c_apps_clk.c, 0x0090 },
		{ &gcc_blsp1_uart2_apps_clk.c, 0x0091 },
		{ &gcc_blsp1_qup3_spi_apps_clk.c, 0x0093 },
		{ &gcc_blsp1_qup3_i2c_apps_clk.c, 0x0094 },
		{ &gcc_blsp1_uart3_apps_clk.c, 0x0095 },
		{ &gcc_blsp1_qup4_spi_apps_clk.c, 0x0098 },
		{ &gcc_blsp1_qup4_i2c_apps_clk.c, 0x0099 },
		{ &gcc_blsp1_uart4_apps_clk.c, 0x009a },
		{ &gcc_blsp1_qup5_spi_apps_clk.c, 0x009c },
		{ &gcc_blsp1_qup5_i2c_apps_clk.c, 0x009d },
		{ &gcc_blsp1_uart5_apps_clk.c, 0x009e },
		{ &gcc_blsp1_qup6_spi_apps_clk.c, 0x00a1 },
		{ &gcc_blsp1_qup6_i2c_apps_clk.c, 0x00a2 },
		{ &gcc_blsp1_uart6_apps_clk.c, 0x00a3 },
		{ &gcc_pdm_ahb_clk.c, 0x00d0 },
		{ &gcc_pdm2_clk.c, 0x00d2 },
		{ &gcc_prng_ahb_clk.c, 0x00d8 },
		{ &gcc_boot_rom_ahb_clk.c, 0x00f8 },
		{ &gcc_crypto_clk.c, 0x0138 },
		{ &gcc_crypto_axi_clk.c, 0x0139 },
		{ &gcc_crypto_ahb_clk.c, 0x013a },
		{ &gcc_apss_ahb_clk.c, 0x0168 },
		{ &gcc_apss_axi_clk.c, 0x0169 },
		{ &gcc_usb_hsic_ahb_clk.c, 0x0198 },
		{ &gcc_usb_hsic_system_clk.c, 0x0199 },
		{ &gcc_usb_hsic_clk.c, 0x019a },
		{ &gcc_usb_hsic_io_cal_clk.c, 0x019b },
		{ &gcc_usb_hsic_io_cal_sleep_clk.c, 0x019c },
		{ &gcc_emac_0_axi_clk.c, 0x01b8 },
		{ &gcc_emac_0_ahb_clk.c, 0x01b9 },
		{ &gcc_emac_0_sys_25m_clk.c, 0x01ba },
		{ &gcc_emac_0_tx_clk.c, 0x01bb },
		{ &gcc_emac_0_125m_clk.c, 0x01bc },
		{ &gcc_emac_0_rx_clk.c, 0x01bd },
		{ &gcc_emac_0_sys_clk.c, 0x01be },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};
/* Clock lookup */
static struct clk_lookup msm_clocks_lookup[] = {
	 CLK_LIST(gpll0_clk_src),
	 CLK_LIST(gpll0_ao_clk_src),
	 CLK_LIST(gpll2_clk_src),
	 CLK_LIST(gpll1_clk_src),
	 CLK_LIST(a7sspll),

	 CLK_LIST(pcnoc_clk),
	 CLK_LIST(pcnoc_a_clk),
	 CLK_LIST(pcnoc_msmbus_clk),
	 CLK_LIST(pcnoc_msmbus_a_clk),
	 CLK_LIST(pcnoc_keepalive_a_clk),
	 CLK_LIST(pcnoc_usb_clk),
	 CLK_LIST(pcnoc_usb_a_clk),
	 CLK_LIST(bimc_clk),
	 CLK_LIST(bimc_a_clk),
	 CLK_LIST(bimc_msmbus_clk),
	 CLK_LIST(bimc_msmbus_a_clk),
	 CLK_LIST(bimc_usb_clk),
	 CLK_LIST(bimc_usb_a_clk),
	 CLK_LIST(qdss_clk),
	 CLK_LIST(qdss_a_clk),
	 CLK_LIST(qpic_clk),
	 CLK_LIST(qpic_a_clk),
	 CLK_LIST(xo_clk_src),
	 CLK_LIST(xo_a_clk_src),
	 CLK_LIST(xo_otg_clk),
	 CLK_LIST(xo_lpm_clk),
	 CLK_LIST(xo_pil_mss_clk),

	 CLK_LIST(bb_clk1),
	 CLK_LIST(bb_clk1_pin),

	 CLK_LIST(gcc_apss_ahb_clk),
	 CLK_LIST(gcc_apss_axi_clk),
	 CLK_LIST(gcc_blsp1_ahb_clk),
	 CLK_LIST(gcc_boot_rom_ahb_clk),
	 CLK_LIST(gcc_crypto_ahb_clk),
	 CLK_LIST(gcc_crypto_axi_clk),
	 CLK_LIST(gcc_crypto_clk),
	 CLK_LIST(gcc_prng_ahb_clk),
	 CLK_LIST(gcc_apss_tcu_clk),
	 CLK_LIST(gcc_qdss_dap_clk),
	 CLK_LIST(gcc_smmu_cfg_clk),
	 CLK_LIST(apss_ahb_clk_src),
	 CLK_LIST(emac_0_125m_clk_src),
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
	 CLK_LIST(crypto_clk_src),
	 CLK_LIST(gp1_clk_src),
	 CLK_LIST(gp2_clk_src),
	 CLK_LIST(gp3_clk_src),
	 CLK_LIST(pdm2_clk_src),
	 CLK_LIST(sdcc1_apps_clk_src),
	 CLK_LIST(sdcc2_apps_clk_src),
	 CLK_LIST(emac_0_sys_25m_clk_src),
	 CLK_LIST(emac_0_tx_clk_src),
	 CLK_LIST(usb_hs_system_clk_src),
	 CLK_LIST(usb_hsic_clk_src),
	 CLK_LIST(usb_hsic_io_cal_clk_src),
	 CLK_LIST(usb_hsic_system_clk_src),
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
	 CLK_LIST(gcc_gp1_clk),
	 CLK_LIST(gcc_gp2_clk),
	 CLK_LIST(gcc_gp3_clk),
	 CLK_LIST(gcc_mss_cfg_ahb_clk),
	 CLK_LIST(gcc_mss_q6_bimc_axi_clk),
	 CLK_LIST(gcc_pdm2_clk),
	 CLK_LIST(gcc_pdm_ahb_clk),
	 CLK_LIST(gcc_sdcc1_ahb_clk),
	 CLK_LIST(gcc_sdcc1_apps_clk),
	 CLK_LIST(gcc_sdcc2_ahb_clk),
	 CLK_LIST(gcc_sdcc2_apps_clk),
	 CLK_LIST(gcc_emac_0_125m_clk),
	 CLK_LIST(gcc_emac_0_ahb_clk),
	 CLK_LIST(gcc_emac_0_axi_clk),
	 CLK_LIST(gcc_emac_0_sys_25m_clk),
	 CLK_LIST(gcc_emac_0_sys_clk),
	 CLK_LIST(gcc_emac_0_tx_clk),
	 CLK_LIST(gcc_emac_0_rx_clk),
	 CLK_LIST(gcc_usb2a_phy_sleep_clk),
	 CLK_LIST(gcc_usb_hs_phy_cfg_ahb_clk),
	 CLK_LIST(gcc_usb_hs_ahb_clk),
	 CLK_LIST(gcc_usb_hs_system_clk),
	 CLK_LIST(gcc_usb_hsic_ahb_clk),
	 CLK_LIST(gcc_usb_hsic_clk),
	 CLK_LIST(gcc_usb_hsic_io_cal_clk),
	 CLK_LIST(gcc_usb_hsic_io_cal_sleep_clk),
	 CLK_LIST(gcc_usb_hsic_system_clk),
	 CLK_LIST(gcc_usb2_hs_phy_only_clk),
	 CLK_LIST(gcc_qusb2_phy_clk),
};

static int msm_gcc_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;
	u32 regval;

	ret = vote_bimc(&bimc_clk, INT_MAX);
	if (ret < 0)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Register base not defined\n");
		return -ENOMEM;
	}

	virt_bases[GCC_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[GCC_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap CC registers\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_base");
	if (!res) {
		dev_err(&pdev->dev, "APCS PLL Register base not defined\n");
		return -ENOMEM;
	}

	virt_bases[APCS_PLL_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[APCS_PLL_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap APCS PLL registers\n");
		return -ENOMEM;
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_dig regulator!!!\n");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	vdd_stromer_pll.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_stromer_dig");
	if (IS_ERR(vdd_stromer_pll.regulator[0])) {
		if (!(PTR_ERR(vdd_stromer_pll.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_stromer_dig regulator!!!\n");
		return PTR_ERR(vdd_stromer_pll.regulator[0]);
	}

	 /*Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_lookup,
				ARRAY_SIZE(msm_clocks_lookup));
	if (ret)
		return ret;

	ret = enable_rpm_scaling();
	if (ret)
		return ret;

	clk_set_rate(&apss_ahb_clk_src.c, 19200000);
	clk_prepare_enable(&apss_ahb_clk_src.c);
	/*
	 * Hold an active set vote for PCNOC AHB source. Sleep set
	 * vote is 0.
	 */
	clk_set_rate(&pcnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pcnoc_keepalive_a_clk.c);

	clk_prepare_enable(&xo_a_clk_src.c);

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-mdm9607" },
	{},
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_probe,
	.driver = {
		.name = "qcom,gcc-mdm9607",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}
arch_initcall(msm_gcc_init);

static struct clk_lookup msm_clocks_measure[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
	CLK_LIST(apss_debug_pri_mux),
	CLK_LIST(apc0_m_clk),
	CLK_LIST(l2_m_clk),
};

static int msm_clock_debug_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	gcc_debug_mux_ops = mux_reg_ops;
	gcc_debug_mux_ops.set_mux_sel = gcc_set_mux_sel;


	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Register base not defined\n");
		return -ENOMEM;
	}

	virt_bases[DEBUG_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[DEBUG_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap debug cc registers\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "meas");
	if (!res) {
		dev_err(&pdev->dev, "GLB clock diag base not defined.\n");
		return -EINVAL;
	}

	meas_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!meas_base) {
		dev_err(&pdev->dev, "Unable to map GLB clock diag base.\n");
		return -ENOMEM;
	}

	ret =  of_msm_clock_register(pdev->dev.of_node, msm_clocks_measure,
					ARRAY_SIZE(msm_clocks_measure));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register debug Mux\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Debug Mux successfully\n");
	return ret;
}

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-mdm9607" },
	{}
};

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_probe,
	.driver = {
		.name = "qcom,cc-debug-mdm9607",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_clock_debug_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}

late_initcall(msm_clock_debug_init);
