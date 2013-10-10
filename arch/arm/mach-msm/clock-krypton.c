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
#include <linux/regulator/consumer.h>
#include <linux/iopoll.h>

#include <mach/clk.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>

#include "clock-local2.h"
#include "clock-pll.h"
#include "clock-rpm.h"
#include "clock-voter.h"
#include "clock.h"

enum {
	GCC_BASE,
	LPASS_BASE,
	APCS_GLB_BASE,
	APCS_GCC_BASE,
	APCS_ACC_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))
#define APCS_GCC_BASE(x) (void __iomem *)(virt_bases[APCS_GCC_BASE] + (x))

/* Mux source select values */
#define xo_source_val	0
#define gpll0_source_val 1
#define gpll1_source_val 4
#define gnd_source_val	5

#define usb3_pipe_clk_source_val	2
#define pcie_pipe_clk_source_val	2

/* Prevent a divider of -1 */
#define FIXDIV(div) (div ? (2 * (div) - 1) : (0))

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(FIXDIV(div))) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_EXT_SRC(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(FIXDIV(div))) \
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

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_BUS_CLK_TYPE	0x316b6c63
#define RPM_MEM_CLK_TYPE	0x326b6c63
#define RPM_QPIC_CLK_TYPE	0x63697071
#define RPM_IPA_CLK_TYPE        0x00617069

#define RPM_SMD_KEY_ENABLE	0x62616E45

#define CXO_ID			0x0
#define QDSS_ID			0x1

#define PNOC_ID		0x0
#define SNOC_ID		0x1
#define CNOC_ID		0x2

#define BIMC_ID		0x0
#define QPIC_ID		0x0
#define IPA_ID		0x0

#define D0_ID		 1
#define D1_ID		 2
#define A0_ID		 3
#define A1_ID		 4
#define A2_ID		 5

#define APCS_CLK_DIAG                                      (0x001C)
#define GPLL0_MODE                                         (0x0000)
#define GPLL1_MODE                                         (0x0040)
#define SYS_NOC_USB3_AXI_CBCR                              (0x0108)
#define MSS_CFG_AHB_CBCR                                   (0x0280)
#define MSS_Q6_BIMC_AXI_CBCR                               (0x0284)
#define USB_30_BCR                                         (0x03C0)
#define USB30_MASTER_CBCR                                  (0x03C8)
#define USB30_SLEEP_CBCR                                   (0x03CC)
#define USB30_MOCK_UTMI_CBCR                               (0x03D0)
#define USB30_MASTER_CMD_RCGR                              (0x03D4)
#define USB30_MOCK_UTMI_CMD_RCGR                           (0x03E8)
#define USB3_PHY_BCR                                       (0x03FC)
#define USB3_PHY_COM_BCR                                   (0x1B88)
#define USB3_PIPE_CBCR                                     (0x1B90)
#define USB3_AUX_CBCR                                      (0x1B94)
#define USB3_PIPE_CMD_RCGR                                 (0x1B98)
#define USB3_AUX_CMD_RCGR                                  (0x1BC0)
#define USB_HS_HSIC_BCR                                    (0x0400)
#define USB_HSIC_AHB_CBCR                                  (0x0408)
#define USB_HSIC_SYSTEM_CMD_RCGR                           (0x041C)
#define USB_HSIC_SYSTEM_CBCR                               (0x040C)
#define USB_HSIC_CMD_RCGR                                  (0x0440)
#define USB_HSIC_CBCR                                      (0x0410)
#define USB_HSIC_IO_CAL_CMD_RCGR                           (0x0458)
#define USB_HSIC_IO_CAL_CBCR                               (0x0414)
#define USB_HSIC_IO_CAL_SLEEP_CBCR                         (0x0418)
#define USB_HSIC_XCVR_FS_CMD_RCGR                          (0x0424)
#define USB_HSIC_XCVR_FS_CBCR                              (0x042C)
#define USB_HS_BCR                                         (0x0480)
#define USB_HS_SYSTEM_CBCR                                 (0x0484)
#define USB_HS_AHB_CBCR                                    (0x0488)
#define USB_HS_SYSTEM_CMD_RCGR                             (0x0490)
#define SDCC2_APPS_CMD_RCGR                                (0x0510)
#define SDCC2_APPS_CBCR                                    (0x0504)
#define SDCC2_AHB_CBCR                                     (0x0508)
#define SDCC3_APPS_CMD_RCGR                                (0x0550)
#define SDCC3_APPS_CBCR                                    (0x0544)
#define SDCC3_AHB_CBCR                                     (0x0548)
#define BLSP1_AHB_CBCR                                     (0x05C4)
#define BLSP1_QUP1_SPI_APPS_CBCR                           (0x0644)
#define BLSP1_QUP1_I2C_APPS_CBCR                           (0x0648)
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR                       (0x0660)
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR                       (0x06E0)
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR                       (0x0760)
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR                       (0x07E0)
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR                       (0x0860)
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR                       (0x08E0)
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR                       (0x064C)
#define BLSP1_UART1_APPS_CBCR                              (0x0684)
#define BLSP1_UART1_APPS_CMD_RCGR                          (0x068C)
#define BLSP1_QUP2_SPI_APPS_CBCR                           (0x06C4)
#define BLSP1_QUP2_I2C_APPS_CBCR                           (0x06C8)
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR                       (0x06CC)
#define BLSP1_UART2_APPS_CBCR                              (0x0704)
#define BLSP1_UART2_APPS_CMD_RCGR                          (0x070C)
#define BLSP1_QUP3_SPI_APPS_CBCR                           (0x0744)
#define BLSP1_QUP3_I2C_APPS_CBCR                           (0x0748)
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR                       (0x074C)
#define BLSP1_UART3_APPS_CBCR                              (0x0784)
#define BLSP1_UART3_APPS_CMD_RCGR                          (0x078C)
#define BLSP1_QUP4_SPI_APPS_CBCR                           (0x07C4)
#define BLSP1_QUP4_I2C_APPS_CBCR                           (0x07C8)
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR                       (0x07CC)
#define BLSP1_UART4_APPS_CBCR                              (0x0804)
#define BLSP1_UART4_APPS_CMD_RCGR                          (0x080C)
#define BLSP1_QUP5_SPI_APPS_CBCR                           (0x0844)
#define BLSP1_QUP5_I2C_APPS_CBCR                           (0x0848)
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR                       (0x084C)
#define BLSP1_UART5_APPS_CBCR                              (0x0884)
#define BLSP1_UART5_APPS_CMD_RCGR                          (0x088C)
#define BLSP1_QUP6_SPI_APPS_CBCR                           (0x08C4)
#define BLSP1_QUP6_I2C_APPS_CBCR                           (0x08C8)
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR                       (0x08CC)
#define BLSP1_UART6_APPS_CBCR                              (0x0904)
#define BLSP1_UART6_APPS_CMD_RCGR                          (0x090C)
#define PDM_AHB_CBCR                                       (0x0CC4)
#define PDM2_CBCR                                          (0x0CCC)
#define PDM2_CMD_RCGR                                      (0x0CD0)
#define PRNG_AHB_CBCR                                      (0x0D04)
#define BAM_DMA_AHB_CBCR                                   (0x0D44)
#define BAM_DMA_INACTIVITY_TIMERS_CBCR                     (0x0D48)
#define BOOT_ROM_AHB_CBCR                                  (0x0E04)
#define RPM_MISC                                           (0x0F24)
#define CE1_CMD_RCGR                                       (0x1050)
#define CE1_CBCR                                           (0x1044)
#define CE1_AXI_CBCR                                       (0x1048)
#define CE1_AHB_CBCR                                       (0x104C)
#define GCC_XO_DIV4_CBCR                                   (0x10C8)
#define LPASS_Q6_AXI_CBCR                                  (0x11C0)
#define APCS_GPLL_ENA_VOTE                                 (0x1480)
#define APCS_CLOCK_BRANCH_ENA_VOTE                         (0x1484)
#define GCC_DEBUG_CLK_CTL                                  (0x1880)
#define CLOCK_FRQ_MEASURE_CTL                              (0x1884)
#define CLOCK_FRQ_MEASURE_STATUS                           (0x1888)
#define PLLTEST_PAD_CFG                                    (0x188C)
#define PCIE_CFG_AHB_CBCR                                  (0x1C04)
#define PCIE_PIPE_CBCR                                     (0x1C08)
#define PCIE_AXI_CBCR                                      (0x1C0C)
#define PCIE_SLEEP_CBCR                                    (0x1C10)
#define PCIE_AXI_MSTR_CBCR                                 (0x1C2C)
#define PCIE_PIPE_CMD_RCGR                                 (0x1C14)
#define PCIE_AUX_CMD_RCGR                                  (0x1E00)
#define PCIE_GPIO_LDO_EN                                   (0x1E40)
#define USB_SS_LDO_EN                                      (0x1E44)
#define Q6SS_AHB_LFABIF_CBCR                               (0x22000)
#define Q6SS_AHBM_CBCR                                     (0x22004)

DEFINE_CLK_RPM_SMD_BRANCH(xo, xo_a_clk, RPM_MISC_CLK_TYPE, CXO_ID, 19200000);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(31),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 600000000,
		.parent = &xo.c,
		.dbg_name = "gpll0",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0.c),
	},
};

/* Don't vote for xo if using this clock to allow xo shutdown */
static struct pll_vote_clk gpll0_ao = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(31),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 600000000,
		.dbg_name = "gpll0_ao",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao.c),
	},
};

static struct pll_vote_clk gpll1 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)GPLL1_MODE,
	.status_mask = BIT(31),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 480000000,
		.parent = &xo.c,
		.dbg_name = "gpll1",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_master_clk[] = {
	F( 125000000,      gpll0,    1,    5,    24),
	F_END
};

static struct rcg_clk usb30_master_clk_src = {
	.cmd_rcgr_reg = USB30_MASTER_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb30_master_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb30_master_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 125000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_i2c_apps_clk[] = {
	F(  19200000,         xo,    1,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_spi_apps_clk[] = {
	F(    960000,         xo,   10,    1,     2),
	F(   4800000,         xo,    4,    0,     0),
	F(   9600000,         xo,    2,    0,     0),
	F(  15000000,      gpll0,   10,    1,     4),
	F(  19200000,         xo,    1,    0,     0),
	F(  25000000,      gpll0,   12,    1,     2),
	F(  50000000,      gpll0,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_uart1_6_apps_clk[] = {
	F(   3686400,      gpll0,    1,   96, 15625),
	F(   7372800,      gpll0,    1,  192, 15625),
	F(  14745600,      gpll0,    1,  384, 15625),
	F(  16000000,      gpll0,    5,    2,    15),
	F(  19200000,         xo,    1,    0,     0),
	F(  24000000,      gpll0,    5,    1,     5),
	F(  32000000,      gpll0,    1,    4,    75),
	F(  40000000,      gpll0,   15,    0,     0),
	F(  46400000,      gpll0,    1,   29,   375),
	F(  48000000,      gpll0, 12.5,    0,     0),
	F(  51200000,      gpll0,    1,   32,   375),
	F(  56000000,      gpll0,    1,    7,    75),
	F(  58982400,      gpll0,    1, 1536, 15625),
	F(  60000000,      gpll0,   10,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart5_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart5_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart6_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart6_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce1_clk[] = {
	F(  50000000,      gpll0,   12,    0,     0),
	F(  85710000,      gpll0,    7,    0,     0),
	F( 100000000,      gpll0,    6,    0,     0),
	F( 171430000,      gpll0,  3.5,    0,     0),
	F( 200000000,      gpll0,    3,    0,     0),
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
		VDD_DIG_FMAX_MAP3(LOW, 85710000, NOMINAL, 171430000, HIGH,
			200000000),
		CLK_INIT(ce1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_sleep_clk[] = {
	F(   1000000,         xo,    1,    5,    96),
	F_END
};

static struct rcg_clk pcie_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_pcie_sleep_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 1000000),
		CLK_INIT(pcie_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_pipe_clk[] = {
	F_EXT_SRC(  62500000, pcie_pipe_clk,    2,    0,     0),
	F_EXT_SRC( 125000000, pcie_pipe_clk,    1,    0,     0),
	F_END
};

static struct rcg_clk pcie_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pcie_pipe_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_pipe_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 62500000, NOMINAL, 125000000),
		CLK_INIT(pcie_pipe_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F(  60000000,      gpll0,   10,    0,     0),
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
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc2_3_apps_clk[] = {
	F(    144000,         xo,   16,    3,    25),
	F(    400000,         xo,   12,    1,     4),
	F(  20000000,      gpll0,   15,    1,     2),
	F(  25000000,      gpll0,   12,    1,     2),
	F(  50000000,      gpll0,   12,    0,     0),
	F( 100000000,      gpll0,    6,    0,     0),
	F( 200000000,      gpll0,    3,    0,     0),
	F_END
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc2_3_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc3_apps_clk_src = {
	.cmd_rcgr_reg = SDCC3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc2_3_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc3_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb3_aux_clk[] = {
	F(   1000000,         xo,    1,    5,    96),
	F_END
};

static struct rcg_clk usb3_aux_clk_src = {
	.cmd_rcgr_reg = USB3_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb3_aux_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb3_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 1000000),
		CLK_INIT(usb3_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb3_pipe_clk[] = {
	F_EXT_SRC( 125000000, usb3_pipe_clk,    1,    0,     0),
	F_END
};

static struct rcg_clk usb3_pipe_clk_src = {
	.cmd_rcgr_reg = USB3_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb3_pipe_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb3_pipe_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 125000000),
		CLK_INIT(usb3_pipe_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_mock_utmi_clk[] = {
	F(  60000000,      gpll0,   10,    0,     0),
	F_END
};

static struct rcg_clk usb30_mock_utmi_clk_src = {
	.cmd_rcgr_reg = USB30_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb30_mock_utmi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb30_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb30_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F(  60000000,      gpll0,   10,    0,     0),
	F(  75000000,      gpll0,    8,    0,     0),
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
		VDD_DIG_FMAX_MAP2(LOW, 60000000, NOMINAL, 75000000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_clk[] = {
	F( 480000000,      gpll1,    1,    0,     0),
	F_END
};

static struct rcg_clk usb_hsic_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 480000000),
		CLK_INIT(usb_hsic_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_io_cal_clk[] = {
	F(   9600000,         xo,    2,    0,     0),
	F_END
};

static struct rcg_clk usb_hsic_io_cal_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_IO_CAL_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_io_cal_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_io_cal_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 9600000),
		CLK_INIT(usb_hsic_io_cal_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_system_clk[] = {
	F(  60000000,      gpll0,   10,    0,     0),
	F(  75000000,      gpll0,    8,    0,     0),
	F_END
};

static struct rcg_clk usb_hsic_system_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_system_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 60000000, NOMINAL, 75000000),
		CLK_INIT(usb_hsic_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_xcvr_fs_clk[] = {
	F(  60000000,      gpll0,   10,    0,     0),
	F_END
};

static struct rcg_clk usb_hsic_xcvr_fs_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_XCVR_FS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_xcvr_fs_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_xcvr_fs_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb_hsic_xcvr_fs_clk_src.c),
	},
};

DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_ID, NULL);

DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_ID, NULL);

static struct gate_clk gcc_pcie_gpio_ldo = {
	.en_reg = PCIE_GPIO_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_gpio_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_pcie_gpio_ldo.c),
	},
};

static struct reset_clk gcc_usb3_phy_com_reset = {
	.reset_reg = USB3_PHY_COM_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb3_phy_com_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb3_phy_com_reset.c),
	},
};

static struct reset_clk gcc_usb3_phy_reset = {
	.reset_reg = USB3_PHY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb3_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb3_phy_reset.c),
	},
};

static struct gate_clk gcc_usb_ss_ldo = {
	.en_reg = USB_SS_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_ss_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_usb_ss_ldo.c),
	},
};

DEFINE_CLK_RPM_SMD(ipa_clk, ipa_a_clk, RPM_IPA_CLK_TYPE, IPA_ID, NULL);

DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_ID, NULL);

DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);

DEFINE_CLK_RPM_SMD(qpic_clk, qpic_a_clk, RPM_QPIC_CLK_TYPE, QPIC_ID, NULL);

DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_ID, NULL);

static struct local_vote_clk gcc_bam_dma_ahb_clk = {
	.cbcr_reg = BAM_DMA_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(12),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bam_dma_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_bam_dma_ahb_clk.c),
	},
};

static struct branch_clk gcc_bam_dma_inactivity_timers_clk = {
	.cbcr_reg = BAM_DMA_INACTIVITY_TIMERS_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bam_dma_inactivity_timers_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bam_dma_inactivity_timers_clk.c),
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
		.dbg_name = "gcc_ce1_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_clk.c),
	},
};

static struct branch_clk gcc_lpass_q6_axi_clk = {
	.cbcr_reg = LPASS_Q6_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_lpass_q6_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_q6_axi_clk.c),
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

static struct branch_clk gcc_pcie_axi_clk = {
	.cbcr_reg = PCIE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_axi_mstr_clk = {
	.cbcr_reg = PCIE_AXI_MSTR_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_axi_mstr_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_axi_mstr_clk.c),
	},
};

static struct branch_clk gcc_pcie_cfg_ahb_clk = {
	.cbcr_reg = PCIE_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_pipe_clk = {
	.cbcr_reg = PCIE_PIPE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_pipe_clk",
		.parent = &pcie_pipe_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_sleep_clk = {
	.cbcr_reg = PCIE_SLEEP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_sleep_clk",
		.parent = &pcie_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_sleep_clk.c),
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

static struct branch_clk gcc_sdcc2_ahb_clk = {
	.cbcr_reg = SDCC2_AHB_CBCR,
	.has_sibling = 1,
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
	.base = &virt_bases[GCC_BASE],
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_apps_clk = {
	.cbcr_reg = SDCC3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc3_apps_clk",
		.parent = &sdcc3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_apps_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_usb3_axi_clk = {
	.cbcr_reg = SYS_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_usb3_aux_clk = {
	.cbcr_reg = USB3_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb3_aux_clk",
		.parent = &usb3_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_aux_clk.c),
	},
};

static struct branch_clk gcc_usb3_pipe_clk = {
	.cbcr_reg = USB3_PIPE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb3_pipe_clk",
		.parent = &usb3_pipe_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_pipe_clk.c),
	},
};

static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = USB30_MASTER_CBCR,
	.bcr_reg = USB_30_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
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
	.base = &virt_bases[GCC_BASE],
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sleep_clk.c),
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
	.has_sibling = 0,
	.bcr_reg = USB_HS_BCR,
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
	.bcr_reg = USB_HS_HSIC_BCR,
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
	.has_sibling = 0,
	.bcr_reg = USB_HS_HSIC_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_system_clk",
		.parent = &usb_hsic_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_system_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_xcvr_fs_clk = {
	.cbcr_reg = USB_HSIC_XCVR_FS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_xcvr_fs_clk",
		.parent = &usb_hsic_xcvr_fs_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_xcvr_fs_clk.c),
	},
};

static struct branch_clk q6ss_ahb_lfabif_clk = {
	.cbcr_reg = Q6SS_AHB_LFABIF_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_ahb_lfabif_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_ahb_lfabif_clk.c),
	},
};

static struct branch_clk q6ss_ahbm_clk = {
	.cbcr_reg = Q6SS_AHBM_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_ahbm_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_ahbm_clk.c),
	},
};

static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, &cnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, &cnoc_a_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_sdcc2_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_sdcc3_clk, &pnoc_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc_clk.c, LONG_MAX);

static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_mss_clk, &xo.c);

static DEFINE_CLK_VOTER(qseecom_ce1_clk_src, &ce1_clk_src.c, 171430000);
static DEFINE_CLK_VOTER(scm_ce1_clk_src, &ce1_clk_src.c, 171430000);
static DEFINE_CLK_VOTER(qcrypto_ce1_clk_src, &ce1_clk_src.c, 171430000);

static DEFINE_CLK_MEASURE(a7_m_clk);

#ifdef CONFIG_DEBUG_FS

struct measure_mux_entry {
	struct clk *c;
	int base;
	u32 debug_mux;
};

struct measure_mux_entry measure_mux[] = {
	{&gcc_mss_cfg_ahb_clk.c,               GCC_BASE, 0x0030},
	{&gcc_mss_q6_bimc_axi_clk.c,           GCC_BASE, 0x0031},
	{&gcc_usb30_master_clk.c,              GCC_BASE, 0x0050},
	{&gcc_usb30_sleep_clk.c,               GCC_BASE, 0x0051},
	{&gcc_usb30_mock_utmi_clk.c,           GCC_BASE, 0x0052},
	{&gcc_usb3_pipe_clk.c,                 GCC_BASE, 0x0054},
	{&gcc_usb3_aux_clk.c,                  GCC_BASE, 0x0055},
	{&gcc_usb_hsic_ahb_clk.c,              GCC_BASE, 0x0058},
	{&gcc_usb_hsic_system_clk.c,           GCC_BASE, 0x0059},
	{&gcc_usb_hsic_clk.c,                  GCC_BASE, 0x005a},
	{&gcc_usb_hsic_io_cal_clk.c,           GCC_BASE, 0x005b},
	{&gcc_usb_hsic_io_cal_sleep_clk.c,     GCC_BASE, 0x005c},
	{&gcc_usb_hsic_xcvr_fs_clk.c,          GCC_BASE, 0x005d},
	{&gcc_usb_hs_system_clk.c,             GCC_BASE, 0x0060},
	{&gcc_usb_hs_ahb_clk.c,                GCC_BASE, 0x0061},
	{&gcc_sdcc2_apps_clk.c,                GCC_BASE, 0x0070},
	{&gcc_sdcc2_ahb_clk.c,                 GCC_BASE, 0x0071},
	{&gcc_sdcc3_apps_clk.c,                GCC_BASE, 0x0078},
	{&gcc_sdcc3_ahb_clk.c,                 GCC_BASE, 0x0079},
	{&gcc_blsp1_ahb_clk.c,                 GCC_BASE, 0x0088},
	{&gcc_blsp1_qup1_spi_apps_clk.c,       GCC_BASE, 0x008a},
	{&gcc_blsp1_qup1_i2c_apps_clk.c,       GCC_BASE, 0x008b},
	{&gcc_blsp1_uart1_apps_clk.c,          GCC_BASE, 0x008c},
	{&gcc_blsp1_qup2_spi_apps_clk.c,       GCC_BASE, 0x008e},
	{&gcc_blsp1_qup2_i2c_apps_clk.c,       GCC_BASE, 0x0090},
	{&gcc_blsp1_uart2_apps_clk.c,          GCC_BASE, 0x0091},
	{&gcc_blsp1_qup3_spi_apps_clk.c,       GCC_BASE, 0x0093},
	{&gcc_blsp1_qup3_i2c_apps_clk.c,       GCC_BASE, 0x0094},
	{&gcc_blsp1_uart3_apps_clk.c,          GCC_BASE, 0x0095},
	{&gcc_blsp1_qup4_spi_apps_clk.c,       GCC_BASE, 0x0098},
	{&gcc_blsp1_qup4_i2c_apps_clk.c,       GCC_BASE, 0x0099},
	{&gcc_blsp1_uart4_apps_clk.c,          GCC_BASE, 0x009a},
	{&gcc_blsp1_qup5_spi_apps_clk.c,       GCC_BASE, 0x009c},
	{&gcc_blsp1_qup5_i2c_apps_clk.c,       GCC_BASE, 0x009d},
	{&gcc_blsp1_uart5_apps_clk.c,          GCC_BASE, 0x009e},
	{&gcc_blsp1_qup6_spi_apps_clk.c,       GCC_BASE, 0x00a1},
	{&gcc_blsp1_qup6_i2c_apps_clk.c,       GCC_BASE, 0x00a2},
	{&gcc_blsp1_uart6_apps_clk.c,          GCC_BASE, 0x00a3},
	{&gcc_pdm_ahb_clk.c,                   GCC_BASE, 0x00d0},
	{&gcc_pdm2_clk.c,                      GCC_BASE, 0x00d2},
	{&gcc_prng_ahb_clk.c,                  GCC_BASE, 0x00d8},
	{&gcc_bam_dma_ahb_clk.c,               GCC_BASE, 0x00e0},
	{&gcc_bam_dma_inactivity_timers_clk.c, GCC_BASE, 0x00e1},
	{&gcc_boot_rom_ahb_clk.c,              GCC_BASE, 0x00f8},
	{&gcc_ce1_clk.c,                       GCC_BASE, 0x0138},
	{&gcc_ce1_axi_clk.c,                   GCC_BASE, 0x0139},
	{&gcc_ce1_ahb_clk.c,                   GCC_BASE, 0x013a},
	{&gcc_lpass_q6_axi_clk.c,              GCC_BASE, 0x0160},
	{&gcc_pcie_cfg_ahb_clk.c,              GCC_BASE, 0x01f0},
	{&gcc_pcie_pipe_clk.c,                 GCC_BASE, 0x01f1},
	{&gcc_pcie_axi_clk.c,                  GCC_BASE, 0x01f2},
	{&gcc_pcie_sleep_clk.c,                GCC_BASE, 0x01f3},
	{&gcc_pcie_axi_mstr_clk.c,             GCC_BASE, 0x01f4},

	{&bimc_clk.c,                          GCC_BASE, 0x0155},
	{&cnoc_clk.c,                          GCC_BASE, 0x0008},
	{&pnoc_clk.c,                          GCC_BASE, 0x0010},
	{&snoc_clk.c,                          GCC_BASE, 0x0000},
	{&ipa_clk.c,                           GCC_BASE, 0x01E0},

	{&q6ss_ahbm_clk.c,                   LPASS_BASE, 0x001d},
	{&q6ss_ahb_lfabif_clk.c,             LPASS_BASE, 0x001e},

	{&a7_m_clk,			  APCS_GCC_BASE,    0x3},
	{&dummy_clk,				N_BASES, 0x0000},
};

static int measure_clk_set_parent(struct clk *c, struct clk *parent)
{
	struct measure_clk *clk = to_measure_clk(c);
	unsigned long flags;
	u32 regval, clk_sel, i;

	if (!parent)
		return -EINVAL;

	for (i = 0; i < (ARRAY_SIZE(measure_mux) - 1); i++)
		if (measure_mux[i].c == parent)
			break;

	if (measure_mux[i].c == &dummy_clk)
		return -EINVAL;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	/*
	 * Program the test vector, measurement period (sample_ticks)
	 * and scaling multiplier.
	 */
	clk->sample_ticks = 0x10000;
	clk->multiplier = 1;

	writel_relaxed(0, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	switch (measure_mux[i].base) {

	case GCC_BASE:
		clk_sel = measure_mux[i].debug_mux;
		break;

	case APCS_GCC_BASE:
		clk_sel = 0x16A;
		regval = BVAL(5, 3, measure_mux[i].debug_mux);
		writel_relaxed(regval, APCS_GCC_BASE(APCS_CLK_DIAG));
		break;

	default:
		return -EINVAL;
	}

	/* Set debug mux clock index */
	regval = BVAL(9, 0, clk_sel);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	/* Activate debug clock output */
	regval |= BIT(16);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	/* Make sure test vector is set before starting measurements. */
	mb();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL));

	/* Wait for timer to become ready. */
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
			BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(20)|ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL));
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
			BIT(25)) == 0)
		cpu_relax();

	/* Return measured ticks. */
	return readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
				BM(24, 0);
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
static unsigned long measure_clk_get_rate(struct clk *c)
{
	unsigned long flags;
	u32 gcc_xo4_reg_backup;
	u64 raw_count_short, raw_count_full;
	struct measure_clk *clk = to_measure_clk(c);
	unsigned ret;

	ret = clk_prepare_enable(&xo.c);
	if (ret) {
		pr_warning("CXO clock failed to enable. Can't measure\n");
		return 0;
	}

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch. */
	gcc_xo4_reg_backup = readl_relaxed(GCC_REG_BASE(GCC_XO_DIV4_CBCR));
	writel_relaxed(0x1, GCC_REG_BASE(GCC_XO_DIV4_CBCR));

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(0x1000);
	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(clk->sample_ticks);

	writel_relaxed(gcc_xo4_reg_backup, GCC_REG_BASE(GCC_XO_DIV4_CBCR));

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short) {
		ret = 0;
	} else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((clk->sample_ticks * 10) + 35));
		ret = (raw_count_full * clk->multiplier);
	}

	writel_relaxed(0x51A00, GCC_REG_BASE(PLLTEST_PAD_CFG));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(&xo.c);

	return ret;
}
#else /* !CONFIG_DEBUG_FS */
static int measure_clk_set_parent(struct clk *clk, struct clk *parent)
{
	return -EINVAL;
}

static unsigned long measure_clk_get_rate(struct clk *clk)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static struct clk_ops clk_ops_measure = {
	.set_parent = measure_clk_set_parent,
	.get_rate = measure_clk_get_rate,
};

static struct measure_clk measure_clk = {
	.c = {
		.dbg_name = "measure_clk",
		.ops = &clk_ops_measure,
		CLK_INIT(measure_clk.c),
	},
	.multiplier = 1,
};

static struct clk_lookup msm_clocks_krypton[] = {
	CLK_LOOKUP("xo",	xo.c,	""),
	CLK_LOOKUP("measure",	measure_clk.c,	"debug"),

	/* PLLS */
	CLK_LOOKUP("",	gpll0.c,	""),
	CLK_LOOKUP("",	gpll1.c,	""),
	CLK_LOOKUP("",  gpll0_ao.c,     ""),

	/* PIL-LPASS */
	CLK_LOOKUP("xo",          cxo_pil_lpass_clk.c, "fe200000.qcom,lpass"),
	CLK_LOOKUP("bus_clk",  gcc_lpass_q6_axi_clk.c, "fe200000.qcom,lpass"),
	CLK_LOOKUP("core_clk",    cxo_pil_lpass_clk.c, "fe200000.qcom,lpass"),
	CLK_LOOKUP("iface_clk", q6ss_ahb_lfabif_clk.c, "fe200000.qcom,lpass"),
	CLK_LOOKUP("reg_clk",         q6ss_ahbm_clk.c, "fe200000.qcom,lpass"),

	/* PIL-MODEM */
	CLK_LOOKUP("xo",              cxo_pil_mss_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("bus_clk", gcc_mss_q6_bimc_axi_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("iface_clk",   gcc_mss_cfg_ahb_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("mem_clk",    gcc_boot_rom_ahb_clk.c, "fc880000.qcom,mss"),

	/* SPS */
	CLK_LOOKUP("dma_bam_pclk", gcc_bam_dma_ahb_clk.c, "msm_sps"),
	CLK_LOOKUP("inactivity_clk", gcc_bam_dma_inactivity_timers_clk.c,
								"msm_sps"),
	CLK_LOOKUP("dfab_clk", pnoc_sps_clk.c, "msm_sps"),

	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9924000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_i2c_apps_clk.c, "f9924000.i2c"),

	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f991f000.serial"),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart3_apps_clk.c, "f991f000.serial"),

	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9928000.spi"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup6_spi_apps_clk.c, "f9928000.spi"),

	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9925000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup3_i2c_apps_clk.c, "f9925000.i2c"),

	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f991d000.uart"),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart1_apps_clk.c, "f991d000.uart"),

	/* IPA */
	CLK_LOOKUP("core_clk",        ipa_clk.c, "fd4c0000.qcom,ipa"),
	CLK_DUMMY("core_src_clk", ipa_clk_src.c, "fd4c0000.qcom,ipa", OFF),
	CLK_DUMMY("bus_clk",  gcc_sys_noc_ipa_axi_clk.c, "fd4c0000.qcom,ipa", OFF),
	CLK_DUMMY("iface_clk",  gcc_ipa_cnoc_clk.c, "fd4c0000.qcom,ipa", OFF),
	CLK_DUMMY("inactivity_clk",  gcc_ipa_sleep_clk.c, "fd4c0000.qcom,ipa", OFF),


	CLK_LOOKUP("core_clk", gcc_blsp1_qup1_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup1_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup3_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup4_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup4_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup5_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup5_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup6_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup6_spi_apps_clk.c, ""),

	CLK_LOOKUP("core_clk", gcc_blsp1_uart2_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart4_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart5_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart6_apps_clk.c, ""),

	CLK_LOOKUP("iface_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng"),

	CLK_LOOKUP("core_clk", gcc_pdm2_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_pdm_ahb_clk.c, ""),

	CLK_LOOKUP("iface_clk", gcc_sdcc2_ahb_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("core_clk", gcc_sdcc2_apps_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("bus_clk",  pnoc_sdcc2_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("iface_clk", gcc_sdcc3_ahb_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("core_clk", gcc_sdcc3_apps_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("bus_clk", pnoc_sdcc3_clk.c, "msm_sdcc.3"),

	CLK_LOOKUP("iface_clk", gcc_usb_hs_ahb_clk.c,     "f9a55000.usb"),
	CLK_LOOKUP("core_clk", gcc_usb_hs_system_clk.c,   "f9a55000.usb"),
	CLK_LOOKUP("iface_clk", gcc_usb_hsic_ahb_clk.c,	  "msm_hsic_host"),
	CLK_LOOKUP("phy_clk", gcc_usb_hsic_clk.c,	  "msm_hsic_host"),
	CLK_LOOKUP("cal_clk", gcc_usb_hsic_io_cal_clk.c,  "msm_hsic_host"),
	CLK_LOOKUP("core_clk", gcc_usb_hsic_system_clk.c, "msm_hsic_host"),
	CLK_LOOKUP("alt_core_clk", gcc_usb_hsic_xcvr_fs_clk.c, ""),
	CLK_LOOKUP("inactivity_clk", gcc_usb_hsic_io_cal_sleep_clk.c,
							"msm_hsic_host"),

	CLK_LOOKUP("core_clk", gcc_ce1_clk.c, "fd400000.qcom,qcedev"),
	CLK_LOOKUP("iface_clk", gcc_ce1_ahb_clk.c, "fd400000.qcom,qcedev"),
	CLK_LOOKUP("bus_clk", gcc_ce1_axi_clk.c, "fd400000.qcom,qcedev"),
	CLK_LOOKUP("core_clk_src", qseecom_ce1_clk_src.c,
						"fd400000.qcom,qcedev"),

	CLK_LOOKUP("core_clk", gcc_ce1_clk.c, "fd400000.qcom,qcrypto"),
	CLK_LOOKUP("iface_clk", gcc_ce1_ahb_clk.c, "fd400000.qcom,qcrypto"),
	CLK_LOOKUP("bus_clk", gcc_ce1_axi_clk.c, "fd400000.qcom,qcrypto"),
	CLK_LOOKUP("core_clk_src", qcrypto_ce1_clk_src.c,
						"fd400000.qcom,qcrypto"),

	CLK_LOOKUP("core_clk",     gcc_ce1_clk.c,         "scm"),
	CLK_LOOKUP("iface_clk",    gcc_ce1_ahb_clk.c,     "scm"),
	CLK_LOOKUP("bus_clk",      gcc_ce1_axi_clk.c,     "scm"),
	CLK_LOOKUP("core_clk_src", scm_ce1_clk_src.c,     "scm"),

	/* RPM and voter clocks */
	CLK_LOOKUP("bus_clk", snoc_clk.c, ""),
	CLK_LOOKUP("bus_clk", pnoc_clk.c, ""),
	CLK_LOOKUP("bus_clk", pnoc_keepalive_a_clk.c, ""),
	CLK_LOOKUP("bus_clk", cnoc_clk.c, ""),
	CLK_LOOKUP("mem_clk", bimc_clk.c, ""),
	CLK_LOOKUP("bus_clk", snoc_a_clk.c, ""),
	CLK_LOOKUP("bus_clk", pnoc_a_clk.c, ""),
	CLK_LOOKUP("bus_clk", cnoc_a_clk.c, ""),
	CLK_LOOKUP("mem_clk", bimc_a_clk.c, ""),

	CLK_LOOKUP("bus_clk",	cnoc_msmbus_clk.c,	"msm_config_noc"),
	CLK_LOOKUP("bus_a_clk",	cnoc_msmbus_a_clk.c,	"msm_config_noc"),
	CLK_LOOKUP("bus_clk",	snoc_msmbus_clk.c,	"msm_sys_noc"),
	CLK_LOOKUP("bus_a_clk",	snoc_msmbus_a_clk.c,	"msm_sys_noc"),
	CLK_LOOKUP("bus_clk",	pnoc_msmbus_clk.c,	"msm_periph_noc"),
	CLK_LOOKUP("bus_a_clk",	pnoc_msmbus_a_clk.c,	"msm_periph_noc"),
	CLK_LOOKUP("mem_clk",	bimc_msmbus_clk.c,	"msm_bimc"),
	CLK_LOOKUP("mem_a_clk",	bimc_msmbus_a_clk.c,	"msm_bimc"),

	CLK_LOOKUP("a7_m_clk", a7_m_clk, ""),

	/* CoreSight clocks */
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc326000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc324000.replicator"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc325000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc323000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc321000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc322000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc302000.stm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc342000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc310000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc311000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc312000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc313000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc314000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc315000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc316000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc317000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc318000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc343000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "f9011038.hwevent"),

	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc326000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc324000.replicator"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc325000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc323000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc321000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc322000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc302000.stm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc342000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc310000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc311000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc312000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc313000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc314000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc315000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc316000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc317000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc318000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc343000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "f9011038.hwevent"),

	/* Misc rcgs without clients */
	CLK_LOOKUP("",	usb30_master_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup1_i2c_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup1_spi_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup2_i2c_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup2_spi_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup3_i2c_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup3_spi_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup4_i2c_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup4_spi_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup5_i2c_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup5_spi_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup6_i2c_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_qup6_spi_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_uart1_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_uart2_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_uart3_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_uart4_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_uart5_apps_clk_src.c,	""),
	CLK_LOOKUP("",	blsp1_uart6_apps_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_aux_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_pipe_clk_src.c,	""),
	CLK_LOOKUP("",	pdm2_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc2_apps_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc3_apps_clk_src.c,	""),
	CLK_LOOKUP("",	usb3_aux_clk_src.c,	""),
	CLK_LOOKUP("",	usb3_pipe_clk_src.c,	""),
	CLK_LOOKUP("",	usb30_mock_utmi_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hs_system_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_io_cal_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_system_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_xcvr_fs_clk_src.c,	""),

	CLK_LOOKUP("",	gcc_pcie_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_axi_mstr_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_pipe_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_sleep_clk.c,	""),
	CLK_LOOKUP("iface_clk",	gcc_sys_noc_usb3_axi_clk.c,
		   "f9200000.qcom,ssusb"),
	CLK_LOOKUP("",	gcc_usb3_aux_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb3_pipe_clk.c,	""),
	CLK_LOOKUP("core_clk",	gcc_usb30_master_clk.c,
		   "f9200000.qcom,ssusb"),
	CLK_LOOKUP("utmi_clk",	gcc_usb30_mock_utmi_clk.c,
		   "f9200000.qcom,ssusb"),
	CLK_LOOKUP("sleep_clk",	gcc_usb30_sleep_clk.c,	"f9200000.qcom,ssusb"),

	CLK_LOOKUP("",	ce1_clk_src.c,	""),
	CLK_LOOKUP("",  gcc_usb3_phy_com_reset.c,       ""),
	CLK_LOOKUP("",  gcc_usb3_phy_reset.c,   ""),
	CLK_LOOKUP("",  gcc_pcie_gpio_ldo.c,   ""),
	CLK_LOOKUP("",  gcc_usb_ss_ldo.c,   ""),
};

static void __init reg_init(void)
{
	u32 regval;

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
}

static void __init msmkrypton_clock_post_init(void)
{
	/*
	 * Hold an active set vote for CXO; this is because CXO is expected
	 * to remain on whenever CPUs aren't power collapsed.
	 */
	clk_prepare_enable(&xo_a_clk.c);

	/*
	 * Hold an active set vote for the PNOC AHB source. Sleep set vote is 0.
	 */
	clk_set_rate(&pnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pnoc_keepalive_a_clk.c);
}

#define GCC_CC_PHYS		0xFC400000
#define GCC_CC_SIZE		SZ_8K

#define LPASS_CC_PHYS		0xFE000000
#define LPASS_CC_SIZE		SZ_256K

#define APCS_GLB_PHYS		0xF9010000
#define APCS_GLB_SIZE		0x38

#define APCS_GCC_PHYS		0xF9011000
#define APCS_GCC_SIZE		0x1C

#define APCS_ACC_PHYS		0xF9008000
#define APCS_ACC_SIZE		0x40

static void __init msmkrypton_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-krypton: Unable to ioremap GCC memory!");

	virt_bases[LPASS_BASE] = ioremap(LPASS_CC_PHYS, LPASS_CC_SIZE);
	if (!virt_bases[LPASS_BASE])
		panic("clock-8226: Unable to ioremap LPASS_CC memory!");

	virt_bases[APCS_GLB_BASE] = ioremap(APCS_GLB_PHYS, APCS_GLB_SIZE);
	if (!virt_bases[APCS_GLB_BASE])
		panic("clock-krypton: Unable to ioremap APCS_GLB memory!");

	virt_bases[APCS_GCC_BASE] = ioremap(APCS_GCC_PHYS, APCS_GCC_SIZE);
	if (!virt_bases[APCS_GCC_BASE])
		panic("clock-krypton: Unable to ioremap APCS_GCC memory!");

	virt_bases[APCS_ACC_BASE] = ioremap(APCS_ACC_PHYS, APCS_ACC_SIZE);
	if (!virt_bases[APCS_ACC_BASE])
		panic("clock-krypton: Unable to ioremap APCS_PLL memory!");

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-krypton: Unable to get the vdd_dig regulator!");

	enable_rpm_scaling();

	reg_init();
}

struct clock_init_data msmkrypton_clock_init_data __initdata = {
	.table = msm_clocks_krypton,
	.size = ARRAY_SIZE(msm_clocks_krypton),
	.pre_init = msmkrypton_clock_pre_init,
	.post_init = msmkrypton_clock_post_init,
};
