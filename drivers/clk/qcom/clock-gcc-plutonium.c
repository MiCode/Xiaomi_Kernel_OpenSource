/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/msm-clocks-plutonium.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>

#include "vdd-level-plutonium.h"

static void __iomem *virt_base;
static void __iomem *virt_dbgbase;

#define GCC_REG_BASE(x) (void __iomem *)(virt_base + (x))

#define gcc_xo_source_val 0
#define gpll0_out_main_source_val 1
#define gpll4_out_main_source_val 5
#define pcie_pipe_source_val 2

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

#define F_EXT(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)FIXDIV(div)) \
			| BVAL(10, 8, s##_source_val), \
	}

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

#define GPLL0_MODE                                       (0x0000)
#define SYS_NOC_USB3_AXI_CBCR                            (0x03FC)
#define SYS_NOC_UFS_AXI_CBCR                             (0x1D7C)
#define MSS_CFG_AHB_CBCR                                 (0x0280)
#define MSS_Q6_BIMC_AXI_CBCR                             (0x0284)
#define USB_30_BCR                                       (0x03C0)
#define USB30_MASTER_CBCR                                (0x03C8)
#define USB30_SLEEP_CBCR                                 (0x03CC)
#define USB30_MOCK_UTMI_CBCR                             (0x03D0)
#define USB30_MASTER_CMD_RCGR                            (0x03D4)
#define USB30_MOCK_UTMI_CMD_RCGR                         (0x03E8)
#define USB3_PHY_AUX_CBCR                                (0x1408)
#define USB3_PHY_PIPE_CBCR                               (0x140C)
#define USB3_PHY_AUX_CMD_RCGR                            (0x1414)
#define USB_HS_BCR                                       (0x0480)
#define USB_HS_SYSTEM_CBCR                               (0x0484)
#define USB_HS_AHB_CBCR                                  (0x0488)
#define USB_HS_SYSTEM_CMD_RCGR                           (0x0490)
#define USB2_HS_PHY_SLEEP_CBCR                           (0x04AC)
#define USB_PHY_CFG_AHB2PHY_CBCR                         (0x1A84)
#define SDCC1_APPS_CMD_RCGR                              (0x04D0)
#define SDCC1_APPS_CBCR                                  (0x04C4)
#define SDCC1_AHB_CBCR                                   (0x04C8)
#define SDCC2_APPS_CMD_RCGR                              (0x0510)
#define SDCC2_APPS_CBCR                                  (0x0504)
#define SDCC2_AHB_CBCR                                   (0x0508)
#define SDCC3_APPS_CMD_RCGR                              (0x0550)
#define SDCC3_APPS_CBCR                                  (0x0544)
#define SDCC3_AHB_CBCR                                   (0x0548)
#define SDCC4_APPS_CMD_RCGR                              (0x0590)
#define SDCC4_APPS_CBCR                                  (0x0584)
#define SDCC4_AHB_CBCR                                   (0x0588)
#define BLSP1_AHB_CBCR                                   (0x05C4)
#define BLSP1_QUP1_SPI_APPS_CBCR                         (0x0644)
#define BLSP1_QUP1_I2C_APPS_CBCR                         (0x0648)
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR                     (0x0660)
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR                     (0x06E0)
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR                     (0x0760)
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR                     (0x07E0)
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR                     (0x0860)
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR                     (0x08E0)
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR                     (0x09A0)
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR                     (0x0A20)
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR                     (0x0AA0)
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR                     (0x0B20)
#define BLSP2_QUP5_I2C_APPS_CMD_RCGR                     (0x0BA0)
#define BLSP2_QUP6_I2C_APPS_CMD_RCGR                     (0x0C20)
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR                     (0x064C)
#define BLSP1_UART1_APPS_CBCR                            (0x0684)
#define BLSP1_UART1_APPS_CMD_RCGR                        (0x068C)
#define BLSP1_QUP2_SPI_APPS_CBCR                         (0x06C4)
#define BLSP1_QUP2_I2C_APPS_CBCR                         (0x06C8)
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR                     (0x06CC)
#define BLSP1_UART2_APPS_CBCR                            (0x0704)
#define BLSP1_UART2_APPS_CMD_RCGR                        (0x070C)
#define BLSP1_QUP3_SPI_APPS_CBCR                         (0x0744)
#define BLSP1_QUP3_I2C_APPS_CBCR                         (0x0748)
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR                     (0x074C)
#define BLSP1_UART3_APPS_CBCR                            (0x0784)
#define BLSP1_UART3_APPS_CMD_RCGR                        (0x078C)
#define BLSP1_QUP4_SPI_APPS_CBCR                         (0x07C4)
#define BLSP1_QUP4_I2C_APPS_CBCR                         (0x07C8)
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR                     (0x07CC)
#define BLSP1_UART4_APPS_CBCR                            (0x0804)
#define BLSP1_UART4_APPS_CMD_RCGR                        (0x080C)
#define BLSP1_QUP5_SPI_APPS_CBCR                         (0x0844)
#define BLSP1_QUP5_I2C_APPS_CBCR                         (0x0848)
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR                     (0x084C)
#define BLSP1_UART5_APPS_CBCR                            (0x0884)
#define BLSP1_UART5_APPS_CMD_RCGR                        (0x088C)
#define BLSP1_QUP6_SPI_APPS_CBCR                         (0x08C4)
#define BLSP1_QUP6_I2C_APPS_CBCR                         (0x08C8)
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR                     (0x08CC)
#define BLSP1_UART6_APPS_CBCR                            (0x0904)
#define BLSP1_UART6_APPS_CMD_RCGR                        (0x090C)
#define BLSP2_AHB_CBCR                                   (0x0944)
#define BLSP2_QUP1_SPI_APPS_CBCR                         (0x0984)
#define BLSP2_QUP1_I2C_APPS_CBCR                         (0x0988)
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR                     (0x098C)
#define BLSP2_UART1_APPS_CBCR                            (0x09C4)
#define BLSP2_UART1_APPS_CMD_RCGR                        (0x09CC)
#define BLSP2_QUP2_SPI_APPS_CBCR                         (0x0A04)
#define BLSP2_QUP2_I2C_APPS_CBCR                         (0x0A08)
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR                     (0x0A0C)
#define BLSP2_UART2_APPS_CBCR                            (0x0A44)
#define BLSP2_UART2_APPS_CMD_RCGR                        (0x0A4C)
#define BLSP2_QUP3_SPI_APPS_CBCR                         (0x0A84)
#define BLSP2_QUP3_I2C_APPS_CBCR                         (0x0A88)
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR                     (0x0A8C)
#define BLSP2_UART3_APPS_CBCR                            (0x0AC4)
#define BLSP2_UART3_APPS_CMD_RCGR                        (0x0ACC)
#define BLSP2_QUP4_SPI_APPS_CBCR                         (0x0B04)
#define BLSP2_QUP4_I2C_APPS_CBCR                         (0x0B08)
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR                     (0x0B0C)
#define BLSP2_UART4_APPS_CBCR                            (0x0B44)
#define BLSP2_UART4_APPS_CMD_RCGR                        (0x0B4C)
#define BLSP2_QUP5_SPI_APPS_CBCR                         (0x0B84)
#define BLSP2_QUP5_I2C_APPS_CBCR                         (0x0B88)
#define BLSP2_QUP5_SPI_APPS_CMD_RCGR                     (0x0B8C)
#define BLSP2_UART5_APPS_CBCR                            (0x0BC4)
#define BLSP2_UART5_APPS_CMD_RCGR                        (0x0BCC)
#define BLSP2_QUP6_SPI_APPS_CBCR                         (0x0C04)
#define BLSP2_QUP6_I2C_APPS_CBCR                         (0x0C08)
#define BLSP2_QUP6_SPI_APPS_CMD_RCGR                     (0x0C0C)
#define BLSP2_UART6_APPS_CBCR                            (0x0C44)
#define BLSP2_UART6_APPS_CMD_RCGR                        (0x0C4C)
#define PDM_AHB_CBCR                                     (0x0CC4)
#define PDM2_CBCR                                        (0x0CCC)
#define PDM2_CMD_RCGR                                    (0x0CD0)
#define PRNG_AHB_CBCR                                    (0x0D04)
#define BAM_DMA_AHB_CBCR                                 (0x0D44)
#define TSIF_AHB_CBCR                                    (0x0D84)
#define TSIF_REF_CBCR                                    (0x0D88)
#define TSIF_REF_CMD_RCGR                                (0x0D90)
#define BOOT_ROM_AHB_CBCR                                (0x0E04)
#define GCC_XO_DIV4_CBCR                                 (0x10C8)
#define LPASS_Q6_AXI_CBCR                                (0x11C0)
#define APCS_GPLL_ENA_VOTE                               (0x1480)
#define APCS_CLOCK_BRANCH_ENA_VOTE                       (0x1484)
#define GCC_DEBUG_CLK_CTL                                (0x1880)
#define CLOCK_FRQ_MEASURE_CTL                            (0x1884)
#define CLOCK_FRQ_MEASURE_STATUS                         (0x1888)
#define PLLTEST_PAD_CFG                                  (0x188C)
#define GP1_CBCR                                         (0x1900)
#define GP1_CMD_RCGR                                     (0x1904)
#define GP2_CBCR                                         (0x1940)
#define GP2_CMD_RCGR                                     (0x1944)
#define GP3_CBCR                                         (0x1980)
#define GP3_CMD_RCGR                                     (0x1984)
#define GPLL4_MODE                                       (0x1DC0)
#define PCIE_0_BCR                                       (0x1AC0)
#define PCIE_0_SLV_AXI_CBCR                              (0x1AC8)
#define PCIE_0_MSTR_AXI_CBCR                             (0x1ACC)
#define PCIE_0_CFG_AHB_CBCR                              (0x1AD0)
#define PCIE_0_AUX_CBCR                                  (0x1AD4)
#define PCIE_0_PIPE_CBCR                                 (0x1AD8)
#define PCIE_0_PIPE_CMD_RCGR                             (0x1ADC)
#define PCIE_0_AUX_CMD_RCGR                              (0x1B00)
#define PCIE_1_BCR                                       (0x1B40)
#define PCIE_1_SLV_AXI_CBCR                              (0x1B48)
#define PCIE_1_MSTR_AXI_CBCR                             (0x1B4C)
#define PCIE_1_CFG_AHB_CBCR                              (0x1B50)
#define PCIE_1_AUX_CBCR                                  (0x1B54)
#define PCIE_1_PIPE_CBCR                                 (0x1B58)
#define PCIE_1_PIPE_CMD_RCGR                             (0x1B5C)
#define PCIE_1_AUX_CMD_RCGR                              (0x1B80)
#define UFS_AXI_CBCR                                     (0x1D48)
#define UFS_AHB_CBCR                                     (0x1D4C)
#define UFS_TX_CFG_CBCR                                  (0x1D50)
#define UFS_RX_CFG_CBCR                                  (0x1D54)
#define UFS_TX_SYMBOL_0_CBCR                             (0x1D58)
#define UFS_TX_SYMBOL_1_CBCR                             (0x1D5C)
#define UFS_RX_SYMBOL_0_CBCR                             (0x1D60)
#define UFS_RX_SYMBOL_1_CBCR                             (0x1D64)
#define UFS_AXI_CMD_RCGR                                 (0x1D68)
#define PCIE_0_PHY_LDO_EN                                (0x1E00)
#define PCIE_1_PHY_LDO_EN                                (0x1E04)
#define USB_SS_PHY_LDO_EN                                (0x1E08)
#define UFS_PHY_LDO_EN                                   (0x1E0C)
#define AXI_CMD_RCGR                                     (0x5040)

DEFINE_EXT_CLK(gcc_xo, NULL);
DEFINE_EXT_CLK(gcc_xo_a_clk, NULL);
DEFINE_EXT_CLK(debug_mmss_clk, NULL);
DEFINE_EXT_CLK(debug_rpm_clk, NULL);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_base,
	.c = {
		.rate = 600000000,
		.parent = &gcc_xo.c,
		.dbg_name = "gpll0",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0.c),
	},
};

static struct pll_vote_clk gpll0_ao = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_base,
	.c = {
		.rate = 600000000,
		.parent = &gcc_xo_a_clk.c,
		.dbg_name = "gpll0_ao",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao.c),
	},
};

DEFINE_EXT_CLK(gpll0_out_main, &gpll0.c);

static struct pll_vote_clk gpll4 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GPLL4_MODE,
	.status_mask = BIT(30),
	.base = &virt_base,
	.c = {
		.rate = 1536000000,
		.parent = &gcc_xo.c,
		.dbg_name = "gpll4",
		.ops = &clk_ops_pll_vote,
		VDD_DIG_FMAX_MAP3(SVS2, 400000000, LOW, 800000000,
				  NOMINAL, 1600000000),
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
	.cmd_rcgr_reg = UFS_AXI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_ufs_axi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "ufs_axi_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP4(SVS2, 50000000, LOW, 100000000,
				  NOMINAL, 200000000, HIGH, 240000000),
		CLK_INIT(ufs_axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb30_master_clk_src[] = {
	F( 125000000, gpll0_out_main,    1,    5,    24),
	F_END
};

static struct rcg_clk usb30_master_clk_src = {
	.cmd_rcgr_reg = USB30_MASTER_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_usb30_master_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb30_master_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(SVS2, 62500000, LOW, 125000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_i2c_apps_clk_src[] = {
	F(  19200000,         gcc_xo,    1,    0,     0),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_spi_apps_clk_src[] = {
	F(    960000,         gcc_xo,   10,    1,     2),
	F(   4800000,         gcc_xo,    4,    0,     0),
	F(   9600000,         gcc_xo,    2,    0,     0),
	F(  15000000, gpll0_out_main,   10,    1,     4),
	F(  19200000,         gcc_xo,    1,    0,     0),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_uart_apps_clk_src[] = {
	F(   3686400, gpll0_out_main,    1,   96, 15625),
	F(   7372800, gpll0_out_main,    1,  192, 15625),
	F(  14745600, gpll0_out_main,    1,  384, 15625),
	F(  16000000, gpll0_out_main,    5,    2,    15),
	F(  19200000,         gcc_xo,    1,    0,     0),
	F(  24000000, gpll0_out_main,    5,    1,     5),
	F(  32000000, gpll0_out_main,    1,    4,    75),
	F(  40000000, gpll0_out_main,   15,    0,     0),
	F(  46400000, gpll0_out_main,    1,   29,   375),
	F(  48000000, gpll0_out_main, 12.5,    0,     0),
	F(  51200000, gpll0_out_main,    1,   32,   375),
	F(  56000000, gpll0_out_main,    1,    7,    75),
	F(  58982400, gpll0_out_main,    1, 1536, 15625),
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F(  63160000, gpll0_out_main,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart5_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart5_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart6_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart6_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup1_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp2_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp2_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp2_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp2_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp2_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 50000000),
		CLK_INIT(blsp2_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp2_qup6_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp2_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp2_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp2_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp2_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart5_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp2_uart5_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart6_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp2_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 15790000, LOW, 31580000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp2_uart6_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp1_clk_src[] = {
	F(  19200000,         gcc_xo,    1,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp2_clk_src[] = {
	F(  19200000,         gcc_xo,    1,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp3_clk_src[] = {
	F(  19200000,         gcc_xo,    1,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp3_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pcie_0_aux_clk_src[] = {
	F(   1011000,         gcc_xo,    1,    1,    19),
	F_END
};

static struct rcg_clk pcie_0_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_0_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_pcie_0_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_0_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(SVS2, 1011000),
		CLK_INIT(pcie_0_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pcie_pipe_clk_src[] = {
	F_EXT( 125000000,      pcie_pipe,    1,    0,     0),
	F_END
};

static struct rcg_clk pcie_0_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_0_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_pcie_pipe_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_0_pipe_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 62500000, LOW, 125000000),
		CLK_INIT(pcie_0_pipe_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pcie_1_aux_clk_src[] = {
	F(   1011000,         gcc_xo,    1,    1,    19),
	F_END
};

static struct rcg_clk pcie_1_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_1_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_pcie_1_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_1_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(SVS2, 1011000),
		CLK_INIT(pcie_1_aux_clk_src.c),
	},
};

static struct rcg_clk pcie_1_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_1_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_pcie_pipe_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_1_pipe_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 62500000, LOW, 125000000),
		CLK_INIT(pcie_1_pipe_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pdm2_clk_src[] = {
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F_END
};

static struct rcg_clk pdm2_clk_src = {
	.cmd_rcgr_reg = PDM2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_pdm2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pdm2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 19200000, LOW, 60000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc1_apps_clk_src[] = {
	F(    144000,         gcc_xo,   16,    3,    25),
	F(    400000,         gcc_xo,   12,    1,     4),
	F(  20000000, gpll0_out_main,   15,    1,     2),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F(  96000000, gpll4_out_main,   16,    0,     0),
	F( 192000000, gpll4_out_main,    8,    0,     0),
	F( 384000000, gpll4_out_main,    4,    0,     0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc1_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 100000000, LOW, 200000000,
				  NOMINAL, 400000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc2_4_apps_clk_src[] = {
	F(    144000,         gcc_xo,   16,    3,    25),
	F(    400000,         gcc_xo,   12,    1,     4),
	F(  20000000, gpll0_out_main,   15,    1,     2),
	F(  25000000, gpll0_out_main,   12,    1,     2),
	F(  50000000, gpll0_out_main,   12,    0,     0),
	F( 100000000, gpll0_out_main,    6,    0,     0),
	F( 200000000, gpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc2_4_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc3_apps_clk_src = {
	.cmd_rcgr_reg = SDCC3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc2_4_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(sdcc3_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc4_apps_clk_src = {
	.cmd_rcgr_reg = SDCC4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc2_4_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(SVS2, 19200000, LOW, 50000000,
				  NOMINAL, 100000000),
		CLK_INIT(sdcc4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_tsif_ref_clk_src[] = {
	F(    105500,         gcc_xo,    1,    1,   182),
	F_END
};

static struct rcg_clk tsif_ref_clk_src = {
	.cmd_rcgr_reg = TSIF_REF_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_tsif_ref_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "tsif_ref_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(SVS2, 105500),
		CLK_INIT(tsif_ref_clk_src.c),
	},
};

DEFINE_FIXED_DIV_CLK(ufs_rx_cfg_postdiv_clk_src, 2, &ufs_axi_clk_src.c);

DEFINE_FIXED_DIV_CLK(ufs_tx_cfg_postdiv_clk_src, 2, &ufs_axi_clk_src.c);

static struct clk_freq_tbl ftbl_usb30_mock_utmi_clk_src[] = {
	F(  60000000, gpll0_out_main,   10,    0,     0),
	F_END
};

static struct rcg_clk usb30_mock_utmi_clk_src = {
	.cmd_rcgr_reg = USB30_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb30_mock_utmi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb30_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(SVS2, 40000000, LOW, 60000000),
		CLK_INIT(usb30_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb3_phy_aux_clk_src[] = {
	F(   1200000,         gcc_xo,   16,    0,     0),
	F_END
};

static struct rcg_clk usb3_phy_aux_clk_src = {
	.cmd_rcgr_reg = USB3_PHY_AUX_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb3_phy_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb3_phy_aux_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(SVS2, 1200000),
		CLK_INIT(usb3_phy_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb_hs_system_clk_src[] = {
	F(  75000000, gpll0_out_main,    8,    0,     0),
	F_END
};

static struct rcg_clk usb_hs_system_clk_src = {
	.cmd_rcgr_reg = USB_HS_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb_hs_system_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb_hs_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(SVS2, 19200000, LOW, 60000000,
				  NOMINAL, 75000000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct gate_clk gpll0_out_mmsscc = {
	.en_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(26),
	.delay_us = 1,
	.base = &virt_base,
	.c = {
		.parent = &gpll0_out_main.c,
		.dbg_name = "gpll0_out_mmsscc",
		.ops = &clk_ops_gate,
		CLK_INIT(gpll0_out_mmsscc.c),
	},
};

static struct gate_clk pcie_0_phy_ldo = {
	.en_reg = PCIE_0_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_0_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(pcie_0_phy_ldo.c),
	},
};

static struct gate_clk pcie_1_phy_ldo = {
	.en_reg = PCIE_1_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_1_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(pcie_1_phy_ldo.c),
	},
};

static struct gate_clk ufs_phy_ldo = {
	.en_reg = UFS_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "ufs_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(ufs_phy_ldo.c),
	},
};

static struct gate_clk usb_ss_phy_ldo = {
	.en_reg = USB_SS_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "usb_ss_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(usb_ss_phy_ldo.c),
	},
};

static struct local_vote_clk gcc_bam_dma_ahb_clk = {
	.cbcr_reg = BAM_DMA_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(12),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_bam_dma_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_bam_dma_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(17),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP2_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP3_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP4_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP5_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP5_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP6_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP1_QUP6_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP1_UART1_APPS_CBCR,
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
	.cbcr_reg = BLSP1_UART2_APPS_CBCR,
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
	.cbcr_reg = BLSP1_UART3_APPS_CBCR,
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
	.cbcr_reg = BLSP1_UART4_APPS_CBCR,
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
	.cbcr_reg = BLSP1_UART5_APPS_CBCR,
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
	.cbcr_reg = BLSP1_UART6_APPS_CBCR,
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
	.cbcr_reg = BLSP2_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(15),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp2_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp2_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP1_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP2_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP2_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP3_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP3_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP4_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP4_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP5_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP5_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP6_I2C_APPS_CBCR,
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
	.cbcr_reg = BLSP2_QUP6_SPI_APPS_CBCR,
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
	.cbcr_reg = BLSP2_UART1_APPS_CBCR,
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
	.cbcr_reg = BLSP2_UART2_APPS_CBCR,
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
	.cbcr_reg = BLSP2_UART3_APPS_CBCR,
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
	.cbcr_reg = BLSP2_UART4_APPS_CBCR,
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
	.cbcr_reg = BLSP2_UART5_APPS_CBCR,
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
	.cbcr_reg = BLSP2_UART6_APPS_CBCR,
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
	.cbcr_reg = BOOT_ROM_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
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
	.cbcr_reg = GP2_CBCR,
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
	.cbcr_reg = GP3_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gp3_clk",
		.parent = &gp3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_lpass_q6_axi_clk = {
	.cbcr_reg = LPASS_Q6_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_lpass_q6_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_q6_axi_clk.c),
	},
};

static struct branch_clk gcc_mss_cfg_ahb_clk = {
	.cbcr_reg = MSS_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_mss_q6_bimc_axi_clk = {
	.cbcr_reg = MSS_Q6_BIMC_AXI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_q6_bimc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_q6_bimc_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_aux_clk = {
	.cbcr_reg = PCIE_0_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_aux_clk",
		.parent = &pcie_0_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_cfg_ahb_clk = {
	.cbcr_reg = PCIE_0_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_mstr_axi_clk = {
	.cbcr_reg = PCIE_0_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_pipe_clk = {
	.cbcr_reg = PCIE_0_PIPE_CBCR,
	.bcr_reg = PCIE_0_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_pipe_clk",
		.parent = &pcie_0_pipe_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_slv_axi_clk = {
	.cbcr_reg = PCIE_0_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_0_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_aux_clk = {
	.cbcr_reg = PCIE_1_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_aux_clk",
		.parent = &pcie_1_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_cfg_ahb_clk = {
	.cbcr_reg = PCIE_1_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_mstr_axi_clk = {
	.cbcr_reg = PCIE_1_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_pipe_clk = {
	.cbcr_reg = PCIE_1_PIPE_CBCR,
	.bcr_reg = PCIE_1_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_pipe_clk",
		.parent = &pcie_1_pipe_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_slv_axi_clk = {
	.cbcr_reg = PCIE_1_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_1_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
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
	.cbcr_reg = PDM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pdm_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_prng_ahb_clk = {
	.cbcr_reg = PRNG_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(13),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_prng_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_prng_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_ahb_clk = {
	.cbcr_reg = SDCC1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_apps_clk = {
	.cbcr_reg = SDCC1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
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
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_apps_clk = {
	.cbcr_reg = SDCC2_APPS_CBCR,
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
	.cbcr_reg = SDCC3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_apps_clk = {
	.cbcr_reg = SDCC3_APPS_CBCR,
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
	.cbcr_reg = SDCC4_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc4_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_apps_clk = {
	.cbcr_reg = SDCC4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc4_apps_clk",
		.parent = &sdcc4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_apps_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_ufs_axi_clk = {
	.cbcr_reg = SYS_NOC_UFS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sys_noc_ufs_axi_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_usb3_axi_clk = {
	.cbcr_reg = SYS_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_tsif_ahb_clk = {
	.cbcr_reg = TSIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_tsif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ahb_clk.c),
	},
};

static struct branch_clk gcc_tsif_ref_clk = {
	.cbcr_reg = TSIF_REF_CBCR,
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
	.cbcr_reg = UFS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_ahb_clk.c),
	},
};

static struct branch_clk gcc_ufs_axi_clk = {
	.cbcr_reg = UFS_AXI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_axi_clk",
		.parent = &ufs_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_cfg_clk = {
	.cbcr_reg = UFS_RX_CFG_CBCR,
	.has_sibling = 1,
	.max_div = 16,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_rx_cfg_clk",
		.parent = &ufs_rx_cfg_postdiv_clk_src.c,
		.ops = &clk_ops_branch,
		.rate = 1,
		CLK_INIT(gcc_ufs_rx_cfg_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_symbol_0_clk = {
	.cbcr_reg = UFS_RX_SYMBOL_0_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_rx_symbol_0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_rx_symbol_0_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_symbol_1_clk = {
	.cbcr_reg = UFS_RX_SYMBOL_1_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_rx_symbol_1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_rx_symbol_1_clk.c),
	},
};

static struct branch_clk gcc_ufs_tx_cfg_clk = {
	.cbcr_reg = UFS_TX_CFG_CBCR,
	.has_sibling = 1,
	.max_div = 16,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_tx_cfg_clk",
		.parent = &ufs_tx_cfg_postdiv_clk_src.c,
		.ops = &clk_ops_branch,
		.rate = 1,
		CLK_INIT(gcc_ufs_tx_cfg_clk.c),
	},
};

static struct branch_clk gcc_ufs_tx_symbol_0_clk = {
	.cbcr_reg = UFS_TX_SYMBOL_0_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_tx_symbol_0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_tx_symbol_0_clk.c),
	},
};

static struct branch_clk gcc_ufs_tx_symbol_1_clk = {
	.cbcr_reg = UFS_TX_SYMBOL_1_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_ufs_tx_symbol_1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_tx_symbol_1_clk.c),
	},
};

static struct branch_clk gcc_usb2_hs_phy_sleep_clk = {
	.cbcr_reg = USB2_HS_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb2_hs_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2_hs_phy_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = USB30_MASTER_CBCR,
	.bcr_reg = USB_30_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_master_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_master_clk.c),
		.depends = &gcc_sys_noc_usb3_axi_clk.c,
	},
};

static struct branch_clk gcc_usb30_mock_utmi_clk = {
	.cbcr_reg = USB30_MOCK_UTMI_CBCR,
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
	.cbcr_reg = USB30_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb3_phy_aux_clk = {
	.cbcr_reg = USB3_PHY_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_phy_aux_clk",
		.parent = &usb3_phy_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_phy_aux_clk.c),
	},
};

static struct branch_clk gcc_usb3_phy_pipe_clk = {
	.cbcr_reg = USB3_PHY_PIPE_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_phy_pipe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_phy_pipe_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_ahb_clk = {
	.cbcr_reg = USB_HS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
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
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb_hs_system_clk",
		.parent = &usb_hs_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
	},
};

static struct branch_clk gcc_usb_phy_cfg_ahb2phy_clk = {
	.cbcr_reg = USB_PHY_CFG_AHB2PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb_phy_cfg_ahb2phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_phy_cfg_ahb2phy_clk.c),
	},
};

static struct mux_clk gcc_debug_mux;
static struct clk_ops clk_ops_debug_mux;
static struct clk_mux_ops gcc_debug_mux_ops;

static struct measure_clk_data debug_mux_priv = {
	.cxo = &gcc_xo.c,
	.plltest_reg = PLLTEST_PAD_CFG,
	.plltest_val = 0x51A00,
	.xo_div4_cbcr = GCC_XO_DIV4_CBCR,
	.ctl_reg = CLOCK_FRQ_MEASURE_CTL,
	.status_reg = CLOCK_FRQ_MEASURE_STATUS,
	.base = &virt_base,
};

static int gcc_set_mux_sel(struct mux_clk *clk, int sel)
{
	u32 regval;

	/* Zero out CDIV bits in top level debug mux register */
	regval = readl_relaxed(GCC_REG_BASE(GCC_DEBUG_CLK_CTL));
	regval &= ~BM(15, 12);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	/*
	 * RPM clocks use the same GCC debug mux. Don't reprogram
	 * the mux (selection) register.
	 */
	if (sel == 0xFFFF)
		return 0;
	mux_reg_ops.set_mux_sel(clk, sel);

	return 0;
}

static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &gcc_debug_mux_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.base = &virt_dbgbase,
	MUX_REC_SRC_LIST(
		&debug_mmss_clk.c,
		&debug_rpm_clk.c,
	),
	MUX_SRC_LIST(
		{ &debug_mmss_clk.c, 0x002b },
		{ &debug_rpm_clk.c, 0xffff },
		{ &gcc_sys_noc_usb3_axi_clk.c, 0x0006 },
		{ &gcc_mss_cfg_ahb_clk.c, 0x0030 },
		{ &gcc_mss_q6_bimc_axi_clk.c, 0x0031 },
		{ &gcc_usb30_master_clk.c, 0x0050 },
		{ &gcc_usb30_sleep_clk.c, 0x0051 },
		{ &gcc_usb30_mock_utmi_clk.c, 0x0052 },
		{ &gcc_usb3_phy_aux_clk.c, 0x0053 },
		{ &gcc_usb3_phy_pipe_clk.c, 0x0054 },
		{ &gcc_sys_noc_ufs_axi_clk.c, 0x0058 },
		{ &gcc_usb_hs_system_clk.c, 0x0060 },
		{ &gcc_usb_hs_ahb_clk.c, 0x0061 },
		{ &gcc_usb2_hs_phy_sleep_clk.c, 0x0063 },
		{ &gcc_usb_phy_cfg_ahb2phy_clk.c, 0x0064 },
		{ &gcc_sdcc1_apps_clk.c, 0x0068 },
		{ &gcc_sdcc1_ahb_clk.c, 0x0069 },
		{ &gcc_sdcc2_apps_clk.c, 0x0070 },
		{ &gcc_sdcc2_ahb_clk.c, 0x0071 },
		{ &gcc_sdcc3_apps_clk.c, 0x0078 },
		{ &gcc_sdcc3_ahb_clk.c, 0x0079 },
		{ &gcc_sdcc4_apps_clk.c, 0x0080 },
		{ &gcc_sdcc4_ahb_clk.c, 0x0081 },
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
		{ &gcc_blsp2_ahb_clk.c, 0x00a8 },
		{ &gcc_blsp2_qup1_spi_apps_clk.c, 0x00aa },
		{ &gcc_blsp2_qup1_i2c_apps_clk.c, 0x00ab },
		{ &gcc_blsp2_uart1_apps_clk.c, 0x00ac },
		{ &gcc_blsp2_qup2_spi_apps_clk.c, 0x00ae },
		{ &gcc_blsp2_qup2_i2c_apps_clk.c, 0x00b0 },
		{ &gcc_blsp2_uart2_apps_clk.c, 0x00b1 },
		{ &gcc_blsp2_qup3_spi_apps_clk.c, 0x00b3 },
		{ &gcc_blsp2_qup3_i2c_apps_clk.c, 0x00b4 },
		{ &gcc_blsp2_uart3_apps_clk.c, 0x00b5 },
		{ &gcc_blsp2_qup4_spi_apps_clk.c, 0x00b8 },
		{ &gcc_blsp2_qup4_i2c_apps_clk.c, 0x00b9 },
		{ &gcc_blsp2_uart4_apps_clk.c, 0x00ba },
		{ &gcc_blsp2_qup5_spi_apps_clk.c, 0x00bc },
		{ &gcc_blsp2_qup5_i2c_apps_clk.c, 0x00bd },
		{ &gcc_blsp2_uart5_apps_clk.c, 0x00be },
		{ &gcc_blsp2_qup6_spi_apps_clk.c, 0x00c1 },
		{ &gcc_blsp2_qup6_i2c_apps_clk.c, 0x00c2 },
		{ &gcc_blsp2_uart6_apps_clk.c, 0x00c3 },
		{ &gcc_pdm_ahb_clk.c, 0x00d0 },
		{ &gcc_pdm2_clk.c, 0x00d2 },
		{ &gcc_prng_ahb_clk.c, 0x00d8 },
		{ &gcc_bam_dma_ahb_clk.c, 0x00e0 },
		{ &gcc_tsif_ahb_clk.c, 0x00e8 },
		{ &gcc_tsif_ref_clk.c, 0x00e9 },
		{ &gcc_boot_rom_ahb_clk.c, 0x00f8 },
		{ &gcc_lpass_q6_axi_clk.c, 0x0160 },
		{ &gcc_pcie_0_slv_axi_clk.c, 0x01e8 },
		{ &gcc_pcie_0_mstr_axi_clk.c, 0x01e9 },
		{ &gcc_pcie_0_cfg_ahb_clk.c, 0x01ea },
		{ &gcc_pcie_0_aux_clk.c, 0x01eb },
		{ &gcc_pcie_0_pipe_clk.c, 0x01ec },
		{ &gcc_pcie_1_slv_axi_clk.c, 0x01f0 },
		{ &gcc_pcie_1_mstr_axi_clk.c, 0x01f1 },
		{ &gcc_pcie_1_cfg_ahb_clk.c, 0x01f2 },
		{ &gcc_pcie_1_aux_clk.c, 0x01f3 },
		{ &gcc_pcie_1_pipe_clk.c, 0x01f4 },
		{ &gcc_ufs_axi_clk.c, 0x0230 },
		{ &gcc_ufs_ahb_clk.c, 0x0231 },
		{ &gcc_ufs_tx_cfg_clk.c, 0x0232 },
		{ &gcc_ufs_rx_cfg_clk.c, 0x0233 },
		{ &gcc_ufs_tx_symbol_0_clk.c, 0x0234 },
		{ &gcc_ufs_tx_symbol_1_clk.c, 0x0235 },
		{ &gcc_ufs_rx_symbol_0_clk.c, 0x0236 },
		{ &gcc_ufs_rx_symbol_1_clk.c, 0x0237 },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_gcc_plutonium[] = {
	CLK_LIST(gcc_xo),
	CLK_LIST(gcc_xo_a_clk),
	CLK_LIST(debug_mmss_clk),
	CLK_LIST(debug_rpm_clk),
	CLK_LIST(gpll0),
	CLK_LIST(gpll0_ao),
	CLK_LIST(gpll0_out_main),
	CLK_LIST(gpll4),
	CLK_LIST(gpll4_out_main),
	CLK_LIST(ufs_axi_clk_src),
	CLK_LIST(usb30_master_clk_src),
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
	CLK_LIST(pcie_0_aux_clk_src),
	CLK_LIST(pcie_0_pipe_clk_src),
	CLK_LIST(pcie_1_aux_clk_src),
	CLK_LIST(pcie_1_pipe_clk_src),
	CLK_LIST(pdm2_clk_src),
	CLK_LIST(sdcc1_apps_clk_src),
	CLK_LIST(sdcc2_apps_clk_src),
	CLK_LIST(sdcc3_apps_clk_src),
	CLK_LIST(sdcc4_apps_clk_src),
	CLK_LIST(tsif_ref_clk_src),
	CLK_LIST(usb30_mock_utmi_clk_src),
	CLK_LIST(usb3_phy_aux_clk_src),
	CLK_LIST(usb_hs_system_clk_src),
	CLK_LIST(gpll0_out_mmsscc),
	CLK_LIST(pcie_0_phy_ldo),
	CLK_LIST(pcie_1_phy_ldo),
	CLK_LIST(ufs_phy_ldo),
	CLK_LIST(usb_ss_phy_ldo),
	CLK_LIST(gcc_bam_dma_ahb_clk),
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
	CLK_LIST(gcc_lpass_q6_axi_clk),
	CLK_LIST(gcc_mss_cfg_ahb_clk),
	CLK_LIST(gcc_mss_q6_bimc_axi_clk),
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
	CLK_LIST(gcc_sys_noc_ufs_axi_clk),
	CLK_LIST(gcc_sys_noc_usb3_axi_clk),
	CLK_LIST(gcc_tsif_ahb_clk),
	CLK_LIST(gcc_tsif_ref_clk),
	CLK_LIST(gcc_ufs_ahb_clk),
	CLK_LIST(gcc_ufs_axi_clk),
	CLK_LIST(gcc_ufs_rx_cfg_clk),
	CLK_LIST(gcc_ufs_rx_symbol_0_clk),
	CLK_LIST(gcc_ufs_rx_symbol_1_clk),
	CLK_LIST(gcc_ufs_tx_cfg_clk),
	CLK_LIST(gcc_ufs_tx_symbol_0_clk),
	CLK_LIST(gcc_ufs_tx_symbol_1_clk),
	CLK_LIST(gcc_usb2_hs_phy_sleep_clk),
	CLK_LIST(gcc_usb30_master_clk),
	CLK_LIST(gcc_usb30_mock_utmi_clk),
	CLK_LIST(gcc_usb30_sleep_clk),
	CLK_LIST(gcc_usb3_phy_aux_clk),
	CLK_LIST(gcc_usb3_phy_pipe_clk),
	CLK_LIST(gcc_usb_hs_ahb_clk),
	CLK_LIST(gcc_usb_hs_system_clk),
	CLK_LIST(gcc_usb_phy_cfg_ahb2phy_clk),
};

static int msm_gcc_plutonium_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *tmp_clk;
	int ret;

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

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get vdd_dig regulator!");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	tmp_clk = gcc_xo.c.parent = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(tmp_clk)) {
		if (!(PTR_ERR(tmp_clk) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get xo clock!");
		return PTR_ERR(tmp_clk);
	}

	tmp_clk = gcc_xo_a_clk.c.parent = devm_clk_get(&pdev->dev, "xo_a_clk");
	if (IS_ERR(tmp_clk)) {
		if (!(PTR_ERR(tmp_clk) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get xo_a_clk clock!");
		return PTR_ERR(tmp_clk);
	}

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_gcc_plutonium,
				    ARRAY_SIZE(msm_clocks_gcc_plutonium));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered GCC clocks.\n");
	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-plutonium" },
	{}
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_plutonium_probe,
	.driver = {
		.name = "qcom,gcc-plutonium",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_gcc_plutonium_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}
arch_initcall(msm_gcc_plutonium_init);

/* ======== Clock Debug Controller ======== */
static struct clk_lookup msm_clocks_measure_plutonium[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
};

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-plutonium" },
	{}
};

static int msm_clock_debug_plutonium_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	gcc_debug_mux_ops = mux_reg_ops;
	gcc_debug_mux_ops.set_mux_sel = gcc_set_mux_sel;

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

	debug_mmss_clk.c.parent = clk_get(&pdev->dev, "debug_mmss_clk");
	if (IS_ERR(debug_mmss_clk.c.parent)) {
		dev_err(&pdev->dev, "Failed to get MMSS debug mux\n");
		return PTR_ERR(debug_mmss_clk.c.parent);
	}

	debug_rpm_clk.c.parent = clk_get(&pdev->dev, "debug_rpm_clk");
	if (IS_ERR(debug_rpm_clk.c.parent)) {
		dev_err(&pdev->dev, "Failed to get RPM debug mux\n");
		return PTR_ERR(debug_rpm_clk.c.parent);
	}

	ret = of_msm_clock_register(pdev->dev.of_node,
				    msm_clocks_measure_plutonium,
				    ARRAY_SIZE(msm_clocks_measure_plutonium));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered debug mux.\n");
	return ret;
}

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_plutonium_probe,
	.driver = {
		.name = "qcom,cc-debug-plutonium",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_clock_debug_plutonium_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}
late_initcall(msm_clock_debug_plutonium_init);
