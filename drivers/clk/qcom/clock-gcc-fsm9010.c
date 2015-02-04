/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/clock-rpm.h>

#include <linux/clk/msm-clock-generic.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/fsm-clocks-9010.h>
#include <dt-bindings/clock/fsm-clocks-hwio-9010.h>

#include "clock.h"

enum {
	GCC_BASE,
	APCS_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))
#define APCS_REG_BASE(x) (void __iomem *)(virt_bases[APCS_BASE] + (x))


/* Mux source select values */
#define xo_source_val	0
#define gpll0_source_val 1
#define gpll1_source_val 2
#define gpll4_source_val 5
#define mpll10_source_val 1
#define gnd_source_val	5
#define sdcc1_gnd_source_val 6
#define pcie_pipe_source_val 2
#define gmac0_source_val 1

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_EXT(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
	},					\
	.num_fmax = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_CORNER_NONE,		/* VDD_DIG_NONE */
	RPM_REGULATOR_CORNER_SVS_SOC,		/* VDD_DIG_LOW */
	RPM_REGULATOR_CORNER_NORMAL,		/* VDD_DIG_NOMINAL */
	RPM_REGULATOR_CORNER_SUPER_TURBO,	/* VDD_DIG_HIGH */
};


static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

/* RPM clocks are never modified in this chip */

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define CXO_ID			0x0

DEFINE_CLK_RPM_SMD_BRANCH(xo_clk_src, xo_a_clk_src,
				RPM_MISC_CLK_TYPE, CXO_ID, 19200000);

static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &xo_clk_src.c);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0_ao_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_ao_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src.c),
	},
};

static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll0_clk_src.c),
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
		.rate = 480000000,
		.dbg_name = "gpll1_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_4_i2c_apps_clk[] = {
	F( 19200000,         xo,    1, 0, 0),
	F( 50000000,      gpll0,   12, 0, 0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_4_spi_apps_clk[] = {
	F(   960000,         xo,   10, 1, 2),
	F(  4800000,         xo,    4, 0, 0),
	F(  9600000,         xo,    2, 0, 0),
	F( 15000000,      gpll0,   10, 1, 4),
	F( 19200000,         xo,    1, 0, 0),
	F( 25000000,      gpll0,   12, 1, 2),
	F( 50000000,      gpll0,   12, 0, 0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_uart1_4_apps_clk[] = {
	F(   660645,         xo,    5, 16, 93),
	F(  3686400,      gpll0,    1, 96, 15625),
	F(  7372800,      gpll0,    1, 192, 15625),
	F( 14745600,      gpll0,    1, 384, 15625),
	F( 16000000,      gpll0,    5, 2, 15),
	F( 19200000,         xo,    1, 0, 0),
	F( 24000000,      gpll0,    5, 1, 5),
	F( 32000000,      gpll0,    1, 4, 75),
	F( 40000000,      gpll0,   15, 0, 0),
	F( 46400000,      gpll0,    1, 29, 375),
	F( 48000000,      gpll0, 12.5, 0, 0),
	F( 51200000,      gpll0,    1, 32, 375),
	F( 56000000,      gpll0,    1, 7, 75),
	F( 58982400,      gpll0,    1, 1536, 15625),
	F( 60000000,      gpll0,   10, 0, 0),
	F( 63160000,      gpll0,  9.5, 0, 0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp_sim_clk[] = {
	F(  3840000,         xo,    5, 0, 0),
	F_END
};

static struct rcg_clk blsp_sim_clk_src = {
	.cmd_rcgr_reg = BLSP_UART_SIM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp_sim_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp_sim_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(blsp_sim_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce1_clk[] = {
	F( 50000000,      gpll0,   12, 0, 0),
	F( 85710000,      gpll0,    7, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(171430000,      gpll0,  3.5, 0, 0),
	F_END
};

static struct rcg_clk ce1_clk_src = {
	.cmd_rcgr_reg = CE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_ce1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ce1_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(ce1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce2_clk[] = {
	F( 50000000,      gpll0,   12, 0, 0),
	F( 85710000,      gpll0,    7, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(171430000,      gpll0,  3.5, 0, 0),
	F_END
};

static struct rcg_clk ce2_clk_src = {
	.cmd_rcgr_reg = CE2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_ce2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ce2_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(ce2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gp1_3_clk[] = {
	F( 19200000,         xo,    1, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(200000000,      gpll0,    3, 0, 0),
	F_END
};

static struct rcg_clk gcc_gp1_clk_src = {
	.cmd_rcgr_reg = GCC_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(gcc_gp1_clk_src.c),
	},
};

static struct rcg_clk gcc_gp2_clk_src = {
	.cmd_rcgr_reg = GCC_GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(gcc_gp2_clk_src.c),
	},
};

static struct rcg_clk gcc_gp3_clk_src = {
	.cmd_rcgr_reg = GCC_GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(gcc_gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_0_1_aux_clk[] = {
	F(  1010000,         xo,    1, 1, 19),
	F_END
};

static struct rcg_clk pcie_0_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_0_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_pcie_0_1_aux_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_0_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(pcie_0_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_0_1_pipe_clk[] = {
	F_EXT(125000000, pcie_pipe,    1, 0, 0),
	F_EXT(250000000, pcie_pipe,    1, 0, 0),
	F_END
};


static struct reset_clk gcc_pcie_phy_0_reset = {
	.reset_reg = PCIE_0_PHY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_phy_0_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_phy_0_reset.c),
	},
};

static struct reset_clk gcc_pcie_phy_1_reset = {
	.reset_reg = PCIE_1_PHY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_phy_1_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_phy_1_reset.c),
	},
};

static struct rcg_clk pcie_0_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_0_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pcie_0_1_pipe_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_0_pipe_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(pcie_0_pipe_clk_src.c),
	},
};

static struct rcg_clk pcie_1_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_1_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_pcie_0_1_aux_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_1_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(pcie_1_aux_clk_src.c),
	},
};

static struct rcg_clk pcie_1_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_1_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pcie_0_1_pipe_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_1_pipe_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(pcie_1_pipe_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F( 60000000,      gpll0,   10, 0, 0),
	F_END
};

static struct rcg_clk pdm2_clk_src = {
	.cmd_rcgr_reg = PDM2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pdm2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pdm2_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc1_4_apps_clk[] = {
	F(   144000,         xo,   16, 3, 25),
	F(   400000,         xo,   12, 1, 4),
	F( 20000000,      gpll0,   15, 1, 2),
	F( 25000000,      gpll0,   12, 1, 2),
	F( 50000000,      gpll0,   12, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(200000000,      gpll0,    3, 0, 0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static DEFINE_CLK_BRANCH_VOTER(xo_usb_hs_host_clk, &xo_clk_src.c);

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F( 75000000,      gpll0,    8, 0, 0),
	F_END
};

static struct rcg_clk usb_hs_system_clk_src = {
	.cmd_rcgr_reg = USB_HS_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hs_system_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hs_system_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup1_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup1_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup2_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup2_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup3_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup3_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup4_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup4_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart1_apps_clk = {
	.cbcr_reg = BLSP1_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart1_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart2_apps_clk = {
	.cbcr_reg = BLSP1_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart2_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart3_apps_clk = {
	.cbcr_reg = BLSP1_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart3_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart4_apps_clk = {
	.cbcr_reg = BLSP1_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart4_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct local_vote_clk gcc_boot_rom_ahb_clk = {
	.cbcr_reg = BOOT_ROM_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_ahb_clk = {
	.cbcr_reg = CE1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(3),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_axi_clk = {
	.cbcr_reg = CE1_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(4),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_axi_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_clk = {
	.cbcr_reg = CE1_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(5),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ce1_clk_src.c,
		.dbg_name = "gcc_ce1_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_clk.c),
	},
};

static struct local_vote_clk gcc_ce2_ahb_clk = {
	.cbcr_reg = CE2_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce2_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce2_axi_clk = {
	.cbcr_reg = CE2_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(1),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce2_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_axi_clk.c),
	},
};

static struct local_vote_clk gcc_ce2_clk = {
	.cbcr_reg = CE2_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(2),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ce2_clk_src.c,
		.dbg_name = "gcc_ce2_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GCC_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_gp1_clk_src.c,
		.dbg_name = "gcc_gp1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GCC_GP2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_gp2_clk_src.c,
		.dbg_name = "gcc_gp2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GCC_GP3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_gp3_clk_src.c,
		.dbg_name = "gcc_gp3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_aux_clk = {
	.cbcr_reg = PCIE_0_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_0_aux_clk_src.c,
		.dbg_name = "gcc_pcie_0_aux_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_cfg_ahb_clk = {
	.cbcr_reg = PCIE_0_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_0_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_mstr_axi_clk = {
	.cbcr_reg = PCIE_0_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_0_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_pipe_clk = {
	.cbcr_reg = PCIE_0_PIPE_CBCR,
	.bcr_reg = PCIEPHY_0_PHY_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_0_pipe_clk_src.c,
		.dbg_name = "gcc_pcie_0_pipe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_slv_axi_clk = {
	.cbcr_reg = PCIE_0_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_0_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_aux_clk = {
	.cbcr_reg = PCIE_1_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_1_aux_clk_src.c,
		.dbg_name = "gcc_pcie_1_aux_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_cfg_ahb_clk = {
	.cbcr_reg = PCIE_1_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_1_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_mstr_axi_clk = {
	.cbcr_reg = PCIE_1_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_1_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_pipe_clk = {
	.cbcr_reg = PCIE_1_PIPE_CBCR,
	.bcr_reg = PCIEPHY_1_PHY_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_1_pipe_clk_src.c,
		.dbg_name = "gcc_pcie_1_pipe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_slv_axi_clk = {
	.cbcr_reg = PCIE_1_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_1_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pdm2_clk_src.c,
		.dbg_name = "gcc_pdm2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm2_clk.c),
	},
};

static struct branch_clk gcc_pdm2_ahb_clk = {
	.cbcr_reg = PDM2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pdm2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm2_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_prng_ahb_clk = {
	.cbcr_reg = PRNG_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(13),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_prng_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_prng_ahb_clk.c),
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
		.parent = &sdcc1_apps_clk_src.c,
		.dbg_name = "gcc_sdcc1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
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

static struct branch_clk gcc_usb_hs_ahb_clk = {
	.cbcr_reg = USB_HS_AHB_CBCR,
	.has_sibling = 1,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hs_system_clk_src.c,
		.dbg_name = "gcc_usb_hs_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
	},
};

static struct branch_clk gcc_ddr_dim_cfg_clk = {
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ddr_dim_cfg_clk",
		.ops = &clk_ops_dummy,
		CLK_INIT(gcc_ddr_dim_cfg_clk.c),
	},
};

static struct branch_clk gcc_ddr_dim_sleep_clk = {
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ddr_dim_sleep_clk",
		.ops = &clk_ops_dummy,
		CLK_INIT(gcc_ddr_dim_sleep_clk.c),
	},
};

static struct branch_clk gcc_gmac0_axi_clk = {
	.cbcr_reg = GMAC1_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac0_axi_clk.c),
	},
};

static struct branch_clk gcc_gmac1_axi_clk = {
	.cbcr_reg = GMAC2_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac1_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac1_axi_clk.c),
	},
};
static struct gate_clk pcie_0_phy_ldo = {
	.en_reg = PCIE_0_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_0_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(pcie_0_phy_ldo.c),
	},
};

static struct gate_clk pcie_1_phy_ldo = {
	.en_reg = PCIE_1_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_1_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(pcie_1_phy_ldo.c),
	},
};

static struct clk_ops clk_ops_debug_mux;

static struct measure_clk_data debug_mux_priv = {
	.cxo = &xo_clk_src.c,
	.plltest_reg = GCC_PLLTEST_PAD_CFG,
	.plltest_val = 0x51A00,
	.xo_div4_cbcr = GCC_XO_DIV4_CBCR,
	.ctl_reg = CLOCK_FRQ_MEASURE_CTL,
	.status_reg = CLOCK_FRQ_MEASURE_STATUS,
	.base = &virt_bases[GCC_BASE],
};

static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &mux_reg_ops,
	.offset = GCC_DEBUG_CLK_CTL,
	.mask = 0x1FF,
	.en_offset = GCC_DEBUG_CLK_CTL,
	.en_mask = BIT(16),
	.base = &virt_bases[GCC_BASE],
	MUX_SRC_LIST(
		{ &gcc_sdcc1_apps_clk.c, 0x0068 },
		{ &gcc_sdcc1_ahb_clk.c, 0x0069 },
		{ &gcc_pdm2_ahb_clk.c, 0x00d0 },
		{ &gcc_pdm2_clk.c, 0x00d2 },
		{ &gcc_prng_ahb_clk.c, 0x00d8 },
		{ &gcc_prng_ahb_clk.c, 0x00d8 },
		{ &gcc_usb_hs_system_clk.c, 0x0060 },
		{ &gcc_usb_hs_ahb_clk.c, 0x0061 },
		{ &gcc_usb2a_phy_sleep_clk.c, 0x0063 },
		{ &gcc_boot_rom_ahb_clk.c, 0x00f8 },
		{ &gcc_ce1_clk.c, 0x0138 },
		{ &gcc_ce1_axi_clk.c, 0x0139 },
		{ &gcc_ce1_ahb_clk.c, 0x013a },
		{ &gcc_ce2_clk.c, 0x0140 },
		{ &gcc_ce2_axi_clk.c, 0x0141 },
		{ &gcc_ce2_ahb_clk.c, 0x0142 },
		{ &gcc_ddr_dim_cfg_clk.c, 0x0160 },
		{ &gcc_ddr_dim_sleep_clk.c, 0x0163 },
		{ &gcc_pcie_0_slv_axi_clk.c, 0x01F8 },
		{ &gcc_pcie_0_mstr_axi_clk.c, 0x01F9 },
		{ &gcc_pcie_0_cfg_ahb_clk.c, 0x01FA },
		{ &gcc_pcie_0_aux_clk.c, 0x01FB },
		{ &gcc_pcie_0_pipe_clk.c, 0x01FC },
		{ &pcie_0_pipe_clk_src.c, 0x01FD },
		{ &gcc_pcie_1_slv_axi_clk.c, 0x0200 },
		{ &gcc_pcie_1_mstr_axi_clk.c, 0x0201 },
		{ &gcc_pcie_1_cfg_ahb_clk.c, 0x0202 },
		{ &gcc_pcie_1_aux_clk.c, 0x0203 },
		{ &gcc_pcie_1_pipe_clk.c, 0x0204 },
		{ &pcie_1_pipe_clk_src.c, 0x0205 },
		{ &pcie_0_phy_ldo.c, 0x02c0 },
		{ &pcie_1_phy_ldo.c, 0x02c8 },
		{ &gcc_gmac0_axi_clk.c, 0x02e5 },
		{ &gcc_gmac1_axi_clk.c, 0x02e6 },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_lookup[] = {

	CLK_LIST(gcc_blsp1_uart4_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_sdcc1_ahb_clk),
	CLK_LIST(gcc_sdcc1_apps_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(pcie_0_phy_ldo),
	CLK_LIST(pcie_1_phy_ldo),
	CLK_LIST(gcc_pcie_0_aux_clk),
	CLK_LIST(gcc_pcie_0_cfg_ahb_clk),
	CLK_LIST(gcc_pcie_0_mstr_axi_clk),
	CLK_LIST(gcc_pcie_0_pipe_clk),
	CLK_LIST(gcc_pcie_0_slv_axi_clk),
	CLK_LIST(gcc_pcie_1_aux_clk),
	CLK_LIST(gcc_pcie_1_cfg_ahb_clk),
	CLK_LIST(gcc_pcie_1_mstr_axi_clk),
	CLK_LIST(gcc_pcie_1_pipe_clk),
	CLK_LIST(gcc_pcie_1_slv_axi_clk),
	CLK_LIST(gcc_ce1_ahb_clk),
	CLK_LIST(gcc_ce1_axi_clk),
	CLK_LIST(gcc_ce1_clk),
	CLK_LIST(ce1_clk_src),
	CLK_LIST(cxo_dwc3_clk),
	CLK_LIST(gcc_pdm2_clk),
	CLK_LIST(gcc_pdm2_ahb_clk),
	CLK_LIST(gcc_pcie_phy_0_reset),
	CLK_LIST(gcc_pcie_phy_1_reset),
};

static int msm_gcc_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;
	u32 regval;

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

	/*Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_lookup,
				ARRAY_SIZE(msm_clocks_lookup));
	if (ret)
		return ret;

	clk_prepare_enable(&xo_a_clk_src.c);

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-fsm9010" },
	{},
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_probe,
	.driver = {
		.name = "qcom,gcc-fsm9010",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}

static struct clk_lookup msm_clocks_measure[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
};

static int msm_clock_debug_probe(struct platform_device *pdev)
{
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

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
	{ .compatible = "qcom,cc-debug-fsm9010" },
	{}
};

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_probe,
	.driver = {
		.name = "qcom,cc-debug-fsm9010",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_clock_debug_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}

arch_initcall(msm_gcc_init);
late_initcall(msm_clock_debug_init);
