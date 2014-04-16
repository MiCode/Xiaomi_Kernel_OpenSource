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
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-voter.h>

#include <dt-bindings/clock/msm-clocks-8974.h>

#include "clock.h"

enum {
	GCC_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))

#define GPLL0_MODE_REG                 0x0000
#define GPLL0_L_REG                    0x0004
#define GPLL0_M_REG                    0x0008
#define GPLL0_N_REG                    0x000C
#define GPLL0_USER_CTL_REG             0x0010
#define GPLL0_CONFIG_CTL_REG           0x0014
#define GPLL0_TEST_CTL_REG             0x0018
#define GPLL0_STATUS_REG               0x001C

#define GPLL1_MODE_REG                 0x0040
#define GPLL1_L_REG                    0x0044
#define GPLL1_M_REG                    0x0048
#define GPLL1_N_REG                    0x004C
#define GPLL1_USER_CTL_REG             0x0050
#define GPLL1_CONFIG_CTL_REG           0x0054
#define GPLL1_TEST_CTL_REG             0x0058
#define GPLL1_STATUS_REG               0x005C

#define GPLL4_MODE_REG                 0x1DC0
#define GPLL4_L_REG                    0x1DC4
#define GPLL4_M_REG                    0x1DC8
#define GPLL4_N_REG                    0x1DCC
#define GPLL4_USER_CTL_REG             0x1DD0
#define GPLL4_CONFIG_CTL_REG           0x1DD4
#define GPLL4_TEST_CTL_REG             0x1DD8
#define GPLL4_STATUS_REG               0x1DDC

#define GCC_DEBUG_CLK_CTL_REG          0x1880
#define CLOCK_FRQ_MEASURE_CTL_REG      0x1884
#define CLOCK_FRQ_MEASURE_STATUS_REG   0x1888
#define GCC_XO_DIV4_CBCR_REG           0x10C8
#define GCC_PLLTEST_PAD_CFG_REG        0x188C
#define APCS_GPLL_ENA_VOTE_REG         0x1480
#define MMSS_PLL_VOTE_APCS_REG         0x0100
#define LPASS_DEBUG_CLK_CTL_REG        0x29000

#define GLB_CLK_DIAG_REG               0x001C
#define L2_CBCR_REG                    0x004C

#define USB30_MASTER_CMD_RCGR          0x03D4
#define USB30_MOCK_UTMI_CMD_RCGR       0x03E8
#define USB_HSIC_SYSTEM_CMD_RCGR       0x041C
#define USB_HSIC_CMD_RCGR              0x0440
#define USB_HSIC_IO_CAL_CMD_RCGR       0x0458
#define USB_HS_SYSTEM_CMD_RCGR         0x0490
#define SYS_NOC_USB3_AXI_CBCR	       0x0108
#define USB30_SLEEP_CBCR	       0x03CC
#define USB2A_PHY_SLEEP_CBCR	       0x04AC
#define USB2B_PHY_SLEEP_CBCR	       0x04B4
#define SDCC1_APPS_CMD_RCGR            0x04D0
#define SDCC2_APPS_CMD_RCGR            0x0510
#define SDCC3_APPS_CMD_RCGR            0x0550
#define SDCC4_APPS_CMD_RCGR            0x0590
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR   0x064C
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR   0x0660
#define BLSP1_UART1_APPS_CMD_RCGR      0x068C
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR   0x06CC
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR   0x06E0
#define BLSP1_UART2_APPS_CMD_RCGR      0x070C
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR   0x074C
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR   0x0760
#define BLSP1_UART3_APPS_CMD_RCGR      0x078C
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR   0x07CC
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR   0x07E0
#define BLSP1_UART4_APPS_CMD_RCGR      0x080C
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR   0x084C
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR   0x0860
#define BLSP1_UART5_APPS_CMD_RCGR      0x088C
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR   0x08CC
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR   0x08E0
#define BLSP1_UART6_APPS_CMD_RCGR      0x090C
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR   0x098C
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR   0x09A0
#define BLSP2_UART1_APPS_CMD_RCGR      0x09CC
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR   0x0A0C
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR   0x0A20
#define BLSP2_UART2_APPS_CMD_RCGR      0x0A4C
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR   0x0A8C
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR   0x0AA0
#define BLSP2_UART3_APPS_CMD_RCGR      0x0ACC
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR   0x0B0C
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR   0x0B20
#define BLSP2_UART4_APPS_CMD_RCGR      0x0B4C
#define BLSP2_QUP5_SPI_APPS_CMD_RCGR   0x0B8C
#define BLSP2_QUP5_I2C_APPS_CMD_RCGR   0x0BA0
#define BLSP2_UART5_APPS_CMD_RCGR      0x0BCC
#define BLSP2_QUP6_SPI_APPS_CMD_RCGR   0x0C0C
#define BLSP2_QUP6_I2C_APPS_CMD_RCGR   0x0C20
#define BLSP2_UART6_APPS_CMD_RCGR      0x0C4C
#define PDM2_CMD_RCGR                  0x0CD0
#define TSIF_REF_CMD_RCGR              0x0D90
#define CE1_CMD_RCGR                   0x1050
#define CE2_CMD_RCGR                   0x1090
#define GP1_CMD_RCGR                   0x1904
#define GP2_CMD_RCGR                   0x1944
#define GP3_CMD_RCGR                   0x1984

#define MMSS_BCR                  0x0240
#define USB_30_BCR                0x03C0
#define USB3_PHY_BCR              0x03FC
#define USB_HS_HSIC_BCR           0x0400
#define USB_HS_BCR                0x0480
#define SDCC1_BCR                 0x04C0
#define SDCC2_BCR                 0x0500
#define SDCC3_BCR                 0x0540
#define SDCC4_BCR                 0x0580
#define BLSP1_BCR                 0x05C0
#define BLSP1_QUP1_BCR            0x0640
#define BLSP1_UART1_BCR           0x0680
#define BLSP1_QUP2_BCR            0x06C0
#define BLSP1_UART2_BCR           0x0700
#define BLSP1_QUP3_BCR            0x0740
#define BLSP1_UART3_BCR           0x0780
#define BLSP1_QUP4_BCR            0x07C0
#define BLSP1_UART4_BCR           0x0800
#define BLSP1_QUP5_BCR            0x0840
#define BLSP1_UART5_BCR           0x0880
#define BLSP1_QUP6_BCR            0x08C0
#define BLSP1_UART6_BCR           0x0900
#define BLSP2_BCR                 0x0940
#define BLSP2_QUP1_BCR            0x0980
#define BLSP2_UART1_BCR           0x09C0
#define BLSP2_QUP2_BCR            0x0A00
#define BLSP2_UART2_BCR           0x0A40
#define BLSP2_QUP3_BCR            0x0A80
#define BLSP2_UART3_BCR           0x0AC0
#define BLSP2_QUP4_BCR            0x0B00
#define BLSP2_UART4_BCR           0x0B40
#define BLSP2_QUP5_BCR            0x0B80
#define BLSP2_UART5_BCR           0x0BC0
#define BLSP2_QUP6_BCR            0x0C00
#define BLSP2_UART6_BCR           0x0C40
#define BOOT_ROM_BCR              0x0E00
#define PDM_BCR                   0x0CC0
#define PRNG_BCR                  0x0D00
#define BAM_DMA_BCR               0x0D40
#define TSIF_BCR                  0x0D80
#define CE1_BCR                   0x1040
#define CE2_BCR                   0x1080

#define OCMEM_SYS_NOC_AXI_CBCR                   0x0244
#define OCMEM_NOC_CFG_AHB_CBCR                   0x0248
#define MMSS_NOC_CFG_AHB_CBCR                    0x024C

#define USB30_MASTER_CBCR                        0x03C8
#define USB30_MOCK_UTMI_CBCR                     0x03D0
#define USB_HSIC_AHB_CBCR                        0x0408
#define USB_HSIC_SYSTEM_CBCR                     0x040C
#define USB_HSIC_CBCR                            0x0410
#define USB_HSIC_IO_CAL_CBCR                     0x0414
#define USB_HS_SYSTEM_CBCR                       0x0484
#define USB_HS_AHB_CBCR                          0x0488
#define SDCC1_APPS_CBCR                          0x04C4
#define SDCC1_AHB_CBCR                           0x04C8
#define SDCC1_CDCCAL_SLEEP_CBCR                  0x04E4
#define SDCC1_CDCCAL_FF_CBCR                     0x04E8
#define SDCC2_APPS_CBCR                          0x0504
#define SDCC2_AHB_CBCR                           0x0508
#define SDCC3_APPS_CBCR                          0x0544
#define SDCC3_AHB_CBCR                           0x0548
#define SDCC4_APPS_CBCR                          0x0584
#define SDCC4_AHB_CBCR                           0x0588
#define BLSP1_AHB_CBCR                           0x05C4
#define BLSP1_QUP1_SPI_APPS_CBCR                 0x0644
#define BLSP1_QUP1_I2C_APPS_CBCR                 0x0648
#define BLSP1_UART1_APPS_CBCR                    0x0684
#define BLSP1_UART1_SIM_CBCR                     0x0688
#define BLSP1_QUP2_SPI_APPS_CBCR                 0x06C4
#define BLSP1_QUP2_I2C_APPS_CBCR                 0x06C8
#define BLSP1_UART2_APPS_CBCR                    0x0704
#define BLSP1_UART2_SIM_CBCR                     0x0708
#define BLSP1_QUP3_SPI_APPS_CBCR                 0x0744
#define BLSP1_QUP3_I2C_APPS_CBCR                 0x0748
#define BLSP1_UART3_APPS_CBCR                    0x0784
#define BLSP1_UART3_SIM_CBCR                     0x0788
#define BLSP1_QUP4_SPI_APPS_CBCR                 0x07C4
#define BLSP1_QUP4_I2C_APPS_CBCR                 0x07C8
#define BLSP1_UART4_APPS_CBCR                    0x0804
#define BLSP1_UART4_SIM_CBCR                     0x0808
#define BLSP1_QUP5_SPI_APPS_CBCR                 0x0844
#define BLSP1_QUP5_I2C_APPS_CBCR                 0x0848
#define BLSP1_UART5_APPS_CBCR                    0x0884
#define BLSP1_UART5_SIM_CBCR                     0x0888
#define BLSP1_QUP6_SPI_APPS_CBCR                 0x08C4
#define BLSP1_QUP6_I2C_APPS_CBCR                 0x08C8
#define BLSP1_UART6_APPS_CBCR                    0x0904
#define BLSP1_UART6_SIM_CBCR                     0x0908
#define BLSP2_AHB_CBCR                           0x0944
#define BOOT_ROM_AHB_CBCR                        0x0E04
#define BLSP2_QUP1_SPI_APPS_CBCR                 0x0984
#define BLSP2_QUP1_I2C_APPS_CBCR                 0x0988
#define BLSP2_UART1_APPS_CBCR                    0x09C4
#define BLSP2_UART1_SIM_CBCR                     0x09C8
#define BLSP2_QUP2_SPI_APPS_CBCR                 0x0A04
#define BLSP2_QUP2_I2C_APPS_CBCR                 0x0A08
#define BLSP2_UART2_APPS_CBCR                    0x0A44
#define BLSP2_UART2_SIM_CBCR                     0x0A48
#define BLSP2_QUP3_SPI_APPS_CBCR                 0x0A84
#define BLSP2_QUP3_I2C_APPS_CBCR                 0x0A88
#define BLSP2_UART3_APPS_CBCR                    0x0AC4
#define BLSP2_UART3_SIM_CBCR                     0x0AC8
#define BLSP2_QUP4_SPI_APPS_CBCR                 0x0B04
#define BLSP2_QUP4_I2C_APPS_CBCR                 0x0B08
#define BLSP2_UART4_APPS_CBCR                    0x0B44
#define BLSP2_UART4_SIM_CBCR                     0x0B48
#define BLSP2_QUP5_SPI_APPS_CBCR                 0x0B84
#define BLSP2_QUP5_I2C_APPS_CBCR                 0x0B88
#define BLSP2_UART5_APPS_CBCR                    0x0BC4
#define BLSP2_UART5_SIM_CBCR                     0x0BC8
#define BLSP2_QUP6_SPI_APPS_CBCR                 0x0C04
#define BLSP2_QUP6_I2C_APPS_CBCR                 0x0C08
#define BLSP2_UART6_APPS_CBCR                    0x0C44
#define BLSP2_UART6_SIM_CBCR                     0x0C48
#define PDM_AHB_CBCR                             0x0CC4
#define PDM_XO4_CBCR                             0x0CC8
#define PDM2_CBCR                                0x0CCC
#define PRNG_AHB_CBCR                            0x0D04
#define BAM_DMA_AHB_CBCR                         0x0D44
#define TSIF_AHB_CBCR                            0x0D84
#define TSIF_REF_CBCR                            0x0D88
#define MSG_RAM_AHB_CBCR                         0x0E44
#define CE1_CBCR                                 0x1044
#define CE1_AXI_CBCR                             0x1048
#define CE1_AHB_CBCR                             0x104C
#define CE2_CBCR                                 0x1084
#define CE2_AXI_CBCR                             0x1088
#define CE2_AHB_CBCR                             0x108C
#define GCC_AHB_CBCR                             0x10C0
#define GP1_CBCR                                 0x1900
#define GP2_CBCR                                 0x1940
#define GP3_CBCR                                 0x1980

#define LPASS_Q6_AXI_CBCR			 0x11C0
#define MSS_CFG_AHB_CBCR                         0x0280
#define MSS_Q6_BIMC_AXI_CBCR			 0x0284

#define APCS_CLOCK_BRANCH_ENA_VOTE 0x1484
#define APCS_CLOCK_SLEEP_ENA_VOTE  0x1488

/* Mux source select values */
#define cxo_source_val	0
#define gpll0_source_val 1
#define gpll1_source_val 2
#define gpll4_source_val 5
#define gnd_source_val	5
#define gpll1_hsic_source_val 4

#define F_GCC_GND \
	{ \
		.freq_hz = 0, \
		.m_val = 0, \
		.n_val  = 0, \
		.div_src_val = BVAL(4, 0, 1) | BVAL(10, 8, gnd_source_val), \
	}

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

#define F_HSIC(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_hsic_source_val), \
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

DEFINE_EXT_CLK(cxo_clk_src, NULL);
DEFINE_EXT_CLK(mmss_debug_clk, NULL);
DEFINE_EXT_CLK(kpss_debug_clk, NULL);
DEFINE_EXT_CLK(rpm_debug_clk, NULL);

DEFINE_CLK_DUMMY(wcnss_m_clk, 0);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0_ao_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS_REG,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 600000000,
		.dbg_name = "gpll0_ao_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src.c),
	},
};

static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS_REG,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_clk_src.c),
	},
};

static struct pll_vote_clk gpll1_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)GPLL1_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.rate = 480000000,
		.dbg_name = "gpll1_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1_clk_src.c),
	},
};

static struct pll_vote_clk gpll4_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GPLL4_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.rate = 768000000,
		.dbg_name = "gpll4_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll4_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_master_clk[] = {
	F(125000000,  gpll0,   1,   5,  24),
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
		VDD_DIG_FMAX_MAP1(NOMINAL, 125000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk[] = {
	F(  960000,    cxo,  10,   1,   2),
	F( 4800000,    cxo,   4,   0,   0),
	F( 9600000,    cxo,   2,   0,   0),
	F(15000000,  gpll0,  10,   1,   4),
	F(19200000,    cxo,   1,   0,   0),
	F(25000000,  gpll0,  12,   1,   2),
	F(50000000,  gpll0,  12,   0,   0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk[] = {
	F(19200000,    cxo,   1,   0,   0),
	F(50000000,  gpll0,  12,   0,   0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_uart1_6_apps_clk[] = {
	F_GCC_GND,
	F( 3686400,  gpll0,    1,  96,  15625),
	F( 7372800,  gpll0,    1, 192,  15625),
	F(14745600,  gpll0,    1, 384,  15625),
	F(16000000,  gpll0,    5,   2,     15),
	F(19200000,    cxo,    1,   0,      0),
	F(24000000,  gpll0,    5,   1,      5),
	F(32000000,  gpll0,    1,   4,     75),
	F(40000000,  gpll0,   15,   0,      0),
	F(46400000,  gpll0,    1,  29,    375),
	F(48000000,  gpll0, 12.5,   0,      0),
	F(51200000,  gpll0,    1,  32,    375),
	F(56000000,  gpll0,    1,   7,     75),
	F(58982400,  gpll0,    1, 1536, 15625),
	F(60000000,  gpll0,   10,   0,      0),
	F(63160000,  gpll0,  9.5,   0,      0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart6_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup6_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup1_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart5_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart5_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart6_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart6_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce1_clk[] = {
	F( 50000000,  gpll0,  12,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_ce1_pro_clk[] = {
	F( 50000000,  gpll0,  12,   0,   0),
	F( 75000000,  gpll0,   8,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F(150000000,  gpll0,   4,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(ce1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce2_clk[] = {
	F( 50000000,  gpll0,  12,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_ce2_pro_clk[] = {
	F( 50000000,  gpll0,  12,   0,   0),
	F( 75000000,  gpll0,   8,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F(150000000,  gpll0,   4,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(ce2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gp_clk[] = {
	F( 4800000,   cxo,  4,  0,   0),
	F( 6000000, gpll0, 10,  1,  10),
	F( 6750000, gpll0,  1,  1,  89),
	F( 8000000, gpll0, 15,  1,   5),
	F( 9600000,   cxo,  2,  0,   0),
	F(16000000, gpll0,  1,  2,  75),
	F(19200000,   cxo,  1,  0,   0),
	F(24000000, gpll0,  5,  1,   5),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F(60000000,  gpll0,  10,   0,   0),
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

/* For MSM8974Pro SDCC1 */
static struct clk_freq_tbl ftbl_gcc_sdcc1_apps_clk_pro[] = {
	F(   144000,    cxo,  16,   3,  25),
	F(   400000,    cxo,  12,   1,   4),
	F( 20000000,  gpll0,  15,   1,   2),
	F( 25000000,  gpll0,  12,   1,   2),
	F( 50000000,  gpll0,  12,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F(192000000,  gpll4,   4,   0,   0),
	F(384000000,  gpll4,   2,   0,   0),
	F_END
};

/* For SDCC1 on MSM8974 v2 and SDCC[2-4] on all MSM8974 */
static struct clk_freq_tbl ftbl_gcc_sdcc1_4_apps_clk[] = {
	F(   144000,    cxo,  16,   3,  25),
	F(   400000,    cxo,  12,   1,   4),
	F( 20000000,  gpll0,  15,   1,   2),
	F( 25000000,  gpll0,  12,   1,   2),
	F( 50000000,  gpll0,  12,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F(200000000,  gpll0,   3,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
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
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc3_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc4_apps_clk_src = {
	.cmd_rcgr_reg = SDCC4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_tsif_ref_clk[] = {
	F(105000,    cxo,   2,   1,  91),
	F_END
};

static struct rcg_clk tsif_ref_clk_src = {
	.cmd_rcgr_reg = TSIF_REF_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_tsif_ref_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "tsif_ref_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 105500),
		CLK_INIT(tsif_ref_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_mock_utmi_clk[] = {
	F(48000000,  gpll0, 12.5,   0,   0),
	F(60000000,  gpll0,   10,   0,   0),
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
		VDD_DIG_FMAX_MAP1(NOMINAL, 60000000),
		CLK_INIT(usb30_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F(75000000,  gpll0,   8,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 37500000, NOMINAL, 75000000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_clk[] = {
	F_HSIC(480000000,  gpll1,   1,   0,   0),
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
	F(9600000,    cxo,   2,   0,   0),
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
	F(75000000,  gpll0,   8,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 37500000, NOMINAL, 75000000),
		CLK_INIT(usb_hsic_system_clk_src.c),
	},
};

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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup4_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup5_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup6_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart1_apps_clk = {
	.cbcr_reg = BLSP1_UART1_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart4_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart5_apps_clk = {
	.cbcr_reg = BLSP1_UART5_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart5_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart6_apps_clk = {
	.cbcr_reg = BLSP1_UART6_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart6_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
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

static struct local_vote_clk gcc_blsp2_ahb_clk = {
	.cbcr_reg = BLSP2_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(15),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp2_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup1_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup1_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup2_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup2_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup3_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup3_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup4_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup4_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup5_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup5_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_I2C_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_SPI_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup6_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup6_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart1_apps_clk = {
	.cbcr_reg = BLSP2_UART1_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart1_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart2_apps_clk = {
	.cbcr_reg = BLSP2_UART2_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart2_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart2_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart3_apps_clk = {
	.cbcr_reg = BLSP2_UART3_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart3_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart3_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart4_apps_clk = {
	.cbcr_reg = BLSP2_UART4_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart4_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart5_apps_clk = {
	.cbcr_reg = BLSP2_UART5_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart5_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart5_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart6_apps_clk = {
	.cbcr_reg = BLSP2_UART6_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart6_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart6_apps_clk.c),
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

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gp1_clk_src.c,
		.dbg_name = "gcc_gp1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GP2_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gp2_clk_src.c,
		.dbg_name = "gcc_gp2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GP3_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gp3_clk_src.c,
		.dbg_name = "gcc_gp3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pdm2_clk_src.c,
		.dbg_name = "gcc_pdm2_clk",
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc1_apps_clk_src.c,
		.dbg_name = "gcc_sdcc1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_cdccal_ff_clk = {
	.cbcr_reg = SDCC1_CDCCAL_FF_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_sdcc1_cdccal_ff_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_cdccal_ff_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_cdccal_sleep_clk = {
	.cbcr_reg = SDCC1_CDCCAL_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_cdccal_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_cdccal_sleep_clk.c),
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc2_apps_clk_src.c,
		.dbg_name = "gcc_sdcc2_apps_clk",
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc3_apps_clk_src.c,
		.dbg_name = "gcc_sdcc3_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_ahb_clk = {
	.cbcr_reg = SDCC4_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc4_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_apps_clk = {
	.cbcr_reg = SDCC4_APPS_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc4_apps_clk_src.c,
		.dbg_name = "gcc_sdcc4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_apps_clk.c),
	},
};

static struct branch_clk gcc_tsif_ahb_clk = {
	.cbcr_reg = TSIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_tsif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ahb_clk.c),
	},
};

static struct branch_clk gcc_tsif_ref_clk = {
	.cbcr_reg = TSIF_REF_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &tsif_ref_clk_src.c,
		.dbg_name = "gcc_tsif_ref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ref_clk.c),
	},
};

struct branch_clk gcc_sys_noc_usb3_axi_clk = {
	.cbcr_reg = SYS_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_master_clk_src.c,
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = USB30_MASTER_CBCR,
	.bcr_reg = USB_30_BCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_master_clk_src.c,
		.dbg_name = "gcc_usb30_master_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_master_clk.c),
		.depends = &gcc_sys_noc_usb3_axi_clk.c,
	},
};

static struct branch_clk gcc_usb30_mock_utmi_clk = {
	.cbcr_reg = USB30_MOCK_UTMI_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_mock_utmi_clk_src.c,
		.dbg_name = "gcc_usb30_mock_utmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_mock_utmi_clk.c),
	},
};

struct branch_clk gcc_usb30_sleep_clk = {
	.cbcr_reg = USB30_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sleep_clk.c),
	},
};

struct branch_clk gcc_usb2a_phy_sleep_clk = {
	.cbcr_reg = USB2A_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2a_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2a_phy_sleep_clk.c),
	},
};

struct branch_clk gcc_usb2b_phy_sleep_clk = {
	.cbcr_reg = USB2B_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2b_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2b_phy_sleep_clk.c),
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hs_system_clk_src.c,
		.dbg_name = "gcc_usb_hs_system_clk",
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
	.bcr_reg = USB_HS_HSIC_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_clk_src.c,
		.dbg_name = "gcc_usb_hsic_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_io_cal_clk = {
	.cbcr_reg = USB_HSIC_IO_CAL_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_io_cal_clk_src.c,
		.dbg_name = "gcc_usb_hsic_io_cal_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_io_cal_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_system_clk = {
	.cbcr_reg = USB_HSIC_SYSTEM_CBCR,
	.bcr_reg = USB_HS_HSIC_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_system_clk_src.c,
		.dbg_name = "gcc_usb_hsic_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_system_clk.c),
	},
};

struct branch_clk gcc_mmss_noc_cfg_ahb_clk = {
	.cbcr_reg = MMSS_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mmss_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_noc_cfg_ahb_clk.c),
	},
};

struct branch_clk gcc_ocmem_noc_cfg_ahb_clk = {
	.cbcr_reg = OCMEM_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ocmem_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ocmem_noc_cfg_ahb_clk.c),
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

static struct mux_clk gcc_debug_mux;
static struct clk_ops clk_ops_debug_mux;

static struct measure_clk_data debug_mux_priv = {
	.cxo = &cxo_clk_src.c,
	.plltest_reg = GCC_PLLTEST_PAD_CFG_REG,
	.plltest_val = 0x51A00,
	.xo_div4_cbcr = GCC_XO_DIV4_CBCR_REG,
	.ctl_reg = CLOCK_FRQ_MEASURE_CTL_REG,
	.status_reg = CLOCK_FRQ_MEASURE_STATUS_REG,
	.base = &virt_bases[GCC_BASE],
};

static int gcc_set_mux_sel(struct mux_clk *clk, int sel)
{
	u32 regval;

	/* Zero out CDIV bits in top level debug mux register */
	regval = readl_relaxed(GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));
	regval &= ~BM(15, 12);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));

	/*
	 * RPM clocks use the same GCC debug mux. Don't reprogram
	 * the mux (selection) register.
	 */
	if (sel == 0xFFFF)
		return 0;
	mux_reg_ops.set_mux_sel(clk, sel);

	return 0;
}

static struct clk_mux_ops gcc_debug_mux_ops;

static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &gcc_debug_mux_ops,
	.offset = GCC_DEBUG_CLK_CTL_REG,
	.en_mask = BIT(16),
	.mask = 0x1FF,
	.base = &virt_bases[GCC_BASE],
	MUX_REC_SRC_LIST(
		&kpss_debug_clk.c,
		&mmss_debug_clk.c,
		&rpm_debug_clk.c,
	),
	MUX_SRC_LIST(
		{&kpss_debug_clk.c,		 0x016A},
		{&mmss_debug_clk.c,		 0x002c},
		{&rpm_debug_clk.c,		 0xFFFF},
		{&gcc_pdm_ahb_clk.c,		 0x00d0},
		{&gcc_blsp2_qup1_i2c_apps_clk.c, 0x00ab},
		{&gcc_blsp2_qup3_spi_apps_clk.c, 0x00b3},
		{&gcc_blsp2_uart5_apps_clk.c,	 0x00be},
		{&gcc_usb30_master_clk.c,	 0x0050},
		{&gcc_blsp2_qup3_i2c_apps_clk.c, 0x00b4},
		{&gcc_usb_hsic_system_clk.c,	 0x0059},
		{&gcc_sdcc1_cdccal_sleep_clk.c,	 0x006a},
		{&gcc_sdcc1_cdccal_ff_clk.c,	 0x006b},
		{&gcc_blsp2_uart3_apps_clk.c,	 0x00b5},
		{&gcc_usb_hsic_io_cal_clk.c,	 0x005b},
		{&gcc_ce2_axi_clk.c,		 0x0141},
		{&gcc_sdcc3_ahb_clk.c,		 0x0079},
		{&gcc_blsp1_qup5_i2c_apps_clk.c, 0x009d},
		{&gcc_blsp1_qup1_spi_apps_clk.c, 0x008a},
		{&gcc_blsp2_uart4_apps_clk.c,	 0x00ba},
		{&gcc_ce2_clk.c,		 0x0140},
		{&gcc_blsp1_uart2_apps_clk.c,	 0x0091},
		{&gcc_sdcc1_ahb_clk.c,		 0x0069},
		{&gcc_mss_cfg_ahb_clk.c,	 0x0030},
		{&gcc_tsif_ahb_clk.c,		 0x00e8},
		{&gcc_sdcc4_ahb_clk.c,		 0x0081},
		{&gcc_blsp1_qup4_spi_apps_clk.c, 0x0098},
		{&gcc_blsp2_qup4_spi_apps_clk.c, 0x00b8},
		{&gcc_blsp1_qup3_spi_apps_clk.c, 0x0093},
		{&gcc_blsp1_qup6_i2c_apps_clk.c, 0x00a2},
		{&gcc_blsp2_qup6_i2c_apps_clk.c, 0x00c2},
		{&gcc_bam_dma_ahb_clk.c,	 0x00e0},
		{&gcc_sdcc3_apps_clk.c,		 0x0078},
		{&gcc_usb_hs_system_clk.c,	 0x0060},
		{&gcc_blsp1_ahb_clk.c,		 0x0088},
		{&gcc_sdcc1_apps_clk.c,		 0x0068},
		{&gcc_blsp2_qup5_i2c_apps_clk.c, 0x00bd},
		{&gcc_blsp1_uart4_apps_clk.c,	 0x009a},
		{&gcc_blsp2_qup2_spi_apps_clk.c, 0x00ae},
		{&gcc_blsp2_qup6_spi_apps_clk.c, 0x00c1},
		{&gcc_blsp2_uart2_apps_clk.c,	 0x00b1},
		{&gcc_blsp1_qup2_spi_apps_clk.c, 0x008e},
		{&gcc_usb_hsic_ahb_clk.c,	 0x0058},
		{&gcc_blsp1_uart3_apps_clk.c,	 0x0095},
		{&gcc_usb30_mock_utmi_clk.c,	 0x0052},
		{&gcc_ce1_axi_clk.c,		 0x0139},
		{&gcc_sdcc4_apps_clk.c,		 0x0080},
		{&gcc_blsp1_qup5_spi_apps_clk.c, 0x009c},
		{&gcc_usb_hs_ahb_clk.c,		 0x0061},
		{&gcc_blsp1_qup6_spi_apps_clk.c, 0x00a1},
		{&gcc_blsp2_qup2_i2c_apps_clk.c, 0x00b0},
		{&gcc_prng_ahb_clk.c,		 0x00d8},
		{&gcc_blsp1_qup3_i2c_apps_clk.c, 0x0094},
		{&gcc_usb_hsic_clk.c,		 0x005a},
		{&gcc_blsp1_uart6_apps_clk.c,	 0x00a3},
		{&gcc_sdcc2_apps_clk.c,		 0x0070},
		{&gcc_tsif_ref_clk.c,		 0x00e9},
		{&gcc_blsp1_uart1_apps_clk.c,	 0x008c},
		{&gcc_blsp2_qup5_spi_apps_clk.c, 0x00bc},
		{&gcc_blsp1_qup4_i2c_apps_clk.c, 0x0099},
		{&gcc_mmss_noc_cfg_ahb_clk.c,	 0x002a},
		{&gcc_blsp2_ahb_clk.c,		 0x00a8},
		{&gcc_boot_rom_ahb_clk.c,	 0x00f8},
		{&gcc_ce1_ahb_clk.c,		 0x013a},
		{&gcc_pdm2_clk.c,		 0x00d2},
		{&gcc_blsp2_qup4_i2c_apps_clk.c, 0x00b9},
		{&gcc_ce2_ahb_clk.c,		 0x0142},
		{&gcc_blsp1_uart5_apps_clk.c,	 0x009e},
		{&gcc_blsp2_qup1_spi_apps_clk.c, 0x00aa},
		{&gcc_blsp1_qup2_i2c_apps_clk.c, 0x0090},
		{&gcc_blsp2_uart1_apps_clk.c,	 0x00ac},
		{&gcc_blsp1_qup1_i2c_apps_clk.c, 0x008b},
		{&gcc_blsp2_uart6_apps_clk.c,	 0x00c3},
		{&gcc_sdcc2_ahb_clk.c,		 0x0071},
		{&gcc_usb30_sleep_clk.c,	 0x0051},
		{&gcc_usb2a_phy_sleep_clk.c,	 0x0063},
		{&gcc_usb2b_phy_sleep_clk.c,	 0x0064},
		{&gcc_sys_noc_usb3_axi_clk.c,	 0x0001},
		{&gcc_ocmem_noc_cfg_ahb_clk.c,	 0x0029},
		{&gcc_ce1_clk.c,		 0x0138},
		{&gcc_lpass_q6_axi_clk.c,	 0x0160},
		{&gcc_mss_q6_bimc_axi_clk.c,	 0x0031},
		{&wcnss_m_clk.c,		 0x0198},
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_gcc_8974pro_only[] = {
	CLK_LOOKUP_OF("gpll4", gpll4_clk_src, ""),
	CLK_LOOKUP_OF("sleep_clk", gcc_sdcc1_cdccal_sleep_clk, "msm_sdcc.1"),
	CLK_LOOKUP_OF("cal_clk", gcc_sdcc1_cdccal_ff_clk, "msm_sdcc.1"),
};

static struct clk_lookup msm_clocks_gcc_8974[] = {
	CLK_LOOKUP_OF("gpll0", gpll0_clk_src,
		   "fd8c0000.qcom,mmsscc"),
	CLK_LOOKUP_OF("aux_clk",   gpll0_ao_clk_src,
						"f9016000.qcom,clock-krait"),
	CLK_LOOKUP_OF("gpll0", gpll0_clk_src, ""),

	CLK_LOOKUP_OF("dma_bam_pclk", gcc_bam_dma_ahb_clk, "msm_sps"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp1_ahb_clk, "f991f000.serial"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp1_ahb_clk, "f9924000.i2c"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp1_ahb_clk, "f991e000.serial"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup1_i2c_apps_clk, "f9923000.i2c"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp1_ahb_clk, "f9923000.i2c"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup2_i2c_apps_clk, "f9924000.i2c"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup2_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup1_spi_apps_clk, "f9923000.spi"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp1_ahb_clk, "f9923000.spi"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup3_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup3_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup4_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup4_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup5_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup5_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup6_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_qup6_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_uart1_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_uart2_apps_clk, "f991e000.serial"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_uart3_apps_clk, "f991f000.serial"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_uart4_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_uart5_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp1_uart6_apps_clk, ""),

	CLK_LOOKUP_OF("iface_clk", gcc_blsp2_ahb_clk, "f9967000.i2c"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp2_ahb_clk, "f9966000.spi"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp2_ahb_clk, "f995e000.serial"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp2_ahb_clk, "f995d000.uart"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup1_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup1_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup2_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup2_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup3_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup3_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup4_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup5_i2c_apps_clk, "f9967000.i2c"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup4_spi_apps_clk, "f9966000.spi"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup5_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup6_i2c_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_qup6_spi_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_uart1_apps_clk, "f995d000.uart"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_uart2_apps_clk, "f995e000.serial"),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_uart3_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_uart4_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_uart5_apps_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_blsp2_uart6_apps_clk, ""),

	CLK_LOOKUP_OF("core_clk_src", ce1_clk_src, ""),
	CLK_LOOKUP_OF("core_clk", gcc_ce1_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_ce2_clk, ""),
	CLK_LOOKUP_OF("iface_clk", gcc_ce1_ahb_clk, ""),
	CLK_LOOKUP_OF("iface_clk", gcc_ce2_ahb_clk, ""),
	CLK_LOOKUP_OF("bus_clk", gcc_ce1_axi_clk, ""),
	CLK_LOOKUP_OF("bus_clk", gcc_ce2_axi_clk, ""),

	CLK_LOOKUP_OF("core_clk",     gcc_ce2_clk,         "qcedev.0"),
	CLK_LOOKUP_OF("iface_clk",    gcc_ce2_ahb_clk,     "qcedev.0"),
	CLK_LOOKUP_OF("bus_clk",      gcc_ce2_axi_clk,     "qcedev.0"),
	CLK_LOOKUP_OF("core_clk_src", ce2_clk_src,         "qcedev.0"),

	CLK_LOOKUP_OF("core_clk",     gcc_ce2_clk,     "qcrypto.0"),
	CLK_LOOKUP_OF("iface_clk",    gcc_ce2_ahb_clk, "qcrypto.0"),
	CLK_LOOKUP_OF("bus_clk",      gcc_ce2_axi_clk, "qcrypto.0"),
	CLK_LOOKUP_OF("core_clk_src", ce2_clk_src,     "qcrypto.0"),

	CLK_LOOKUP_OF("core_clk",     gcc_ce1_clk,         "qseecom"),
	CLK_LOOKUP_OF("iface_clk",    gcc_ce1_ahb_clk,     "qseecom"),
	CLK_LOOKUP_OF("bus_clk",      gcc_ce1_axi_clk,     "qseecom"),
	CLK_LOOKUP_OF("core_clk_src", ce1_clk_src,         "qseecom"),

	CLK_LOOKUP_OF("ce_drv_core_clk",     gcc_ce2_clk,         "qseecom"),
	CLK_LOOKUP_OF("ce_drv_iface_clk",    gcc_ce2_ahb_clk,     "qseecom"),
	CLK_LOOKUP_OF("ce_drv_bus_clk",      gcc_ce2_axi_clk,     "qseecom"),
	CLK_LOOKUP_OF("ce_drv_core_clk_src", ce2_clk_src,         "qseecom"),

	CLK_LOOKUP_OF("core_clk",     gcc_ce1_clk,         "mcd"),
	CLK_LOOKUP_OF("iface_clk",    gcc_ce1_ahb_clk,     "mcd"),
	CLK_LOOKUP_OF("bus_clk",      gcc_ce1_axi_clk,     "mcd"),
	CLK_LOOKUP_OF("core_clk_src", ce1_clk_src,         "mcd"),

	CLK_LOOKUP_OF("core_clk",     gcc_ce1_clk,         "scm"),
	CLK_LOOKUP_OF("iface_clk",    gcc_ce1_ahb_clk,     "scm"),
	CLK_LOOKUP_OF("bus_clk",      gcc_ce1_axi_clk,     "scm"),
	CLK_LOOKUP_OF("core_clk_src", ce1_clk_src,         "scm"),

	CLK_LOOKUP_OF("core_clk", gcc_gp1_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_gp2_clk, ""),
	CLK_LOOKUP_OF("core_clk", gcc_gp3_clk, ""),

	CLK_LOOKUP_OF("core_clk", gcc_pdm2_clk, ""),
	CLK_LOOKUP_OF("iface_clk", gcc_pdm_ahb_clk, ""),
	CLK_LOOKUP_OF("iface_clk", gcc_prng_ahb_clk, ""),

	CLK_LOOKUP_OF("iface_clk", gcc_sdcc1_ahb_clk, "msm_sdcc.1"),
	CLK_LOOKUP_OF("core_clk", gcc_sdcc1_apps_clk, "msm_sdcc.1"),
	CLK_LOOKUP_OF("iface_clk", gcc_sdcc2_ahb_clk, "msm_sdcc.2"),
	CLK_LOOKUP_OF("core_clk", gcc_sdcc2_apps_clk, "msm_sdcc.2"),
	CLK_LOOKUP_OF("iface_clk", gcc_sdcc3_ahb_clk, "msm_sdcc.3"),
	CLK_LOOKUP_OF("core_clk", gcc_sdcc3_apps_clk, "msm_sdcc.3"),
	CLK_LOOKUP_OF("iface_clk", gcc_sdcc4_ahb_clk, "msm_sdcc.4"),
	CLK_LOOKUP_OF("core_clk", gcc_sdcc4_apps_clk, "msm_sdcc.4"),

	CLK_LOOKUP_OF("iface_clk", gcc_tsif_ahb_clk, "f99d8000.msm_tspp"),
	CLK_LOOKUP_OF("ref_clk", gcc_tsif_ref_clk, "f99d8000.msm_tspp"),

	CLK_LOOKUP_OF("mem_clk", gcc_usb30_master_clk,           "usb_bam"),
	CLK_LOOKUP_OF("mem_iface_clk", gcc_sys_noc_usb3_axi_clk, "usb_bam"),
	CLK_LOOKUP_OF("core_clk", gcc_usb30_master_clk,    "msm_dwc3"),
	CLK_LOOKUP_OF("utmi_clk_src", usb30_mock_utmi_clk_src, "msm_dwc3"),
	CLK_LOOKUP_OF("utmi_clk", gcc_usb30_mock_utmi_clk, "msm_dwc3"),
	CLK_LOOKUP_OF("iface_clk", gcc_sys_noc_usb3_axi_clk, "msm_dwc3"),
	CLK_LOOKUP_OF("iface_clk", gcc_sys_noc_usb3_axi_clk, "msm_usb3"),
	CLK_LOOKUP_OF("sleep_clk", gcc_usb30_sleep_clk, "msm_dwc3"),
	CLK_LOOKUP_OF("sleep_a_clk", gcc_usb2a_phy_sleep_clk, "msm_dwc3"),
	CLK_LOOKUP_OF("sleep_b_clk", gcc_usb2b_phy_sleep_clk, "msm_dwc3"),
	CLK_LOOKUP_OF("iface_clk", gcc_usb_hs_ahb_clk,     "msm_otg"),
	CLK_LOOKUP_OF("core_clk", gcc_usb_hs_system_clk,   "msm_otg"),
	CLK_LOOKUP_OF("iface_clk", gcc_usb_hsic_ahb_clk,  "msm_hsic_host"),
	CLK_LOOKUP_OF("phy_clk", gcc_usb_hsic_clk,	  "msm_hsic_host"),
	CLK_LOOKUP_OF("cal_clk", gcc_usb_hsic_io_cal_clk,  "msm_hsic_host"),
	CLK_LOOKUP_OF("core_clk", gcc_usb_hsic_system_clk, "msm_hsic_host"),
	CLK_LOOKUP_OF("iface_clk", gcc_usb_hs_ahb_clk,     "msm_ehci_host"),
	CLK_LOOKUP_OF("core_clk", gcc_usb_hs_system_clk,   "msm_ehci_host"),
	CLK_LOOKUP_OF("sleep_clk", gcc_usb2b_phy_sleep_clk, "msm_ehci_host"),

	CLK_LOOKUP_OF("bus_clk", gcc_mss_q6_bimc_axi_clk, "fc880000.qcom,mss"),
	CLK_LOOKUP_OF("iface_clk", gcc_mss_cfg_ahb_clk, "fc880000.qcom,mss"),
	CLK_LOOKUP_OF("mem_clk", gcc_boot_rom_ahb_clk,  "fc880000.qcom,mss"),
	CLK_LOOKUP_OF("core_clk", gcc_prng_ahb_clk, "msm_rng"),

	CLK_LOOKUP_OF("iface_clk", gcc_mmss_noc_cfg_ahb_clk, ""),
	CLK_LOOKUP_OF("iface_clk", gcc_ocmem_noc_cfg_ahb_clk, ""),

	CLK_LOOKUP_OF("bus_clk", gcc_lpass_q6_axi_clk,  "fe200000.qcom,lpass"),
	CLK_LOOKUP_OF("iface_clk", gcc_blsp1_ahb_clk, "f9928000.i2c"),
	CLK_LOOKUP_OF("core_clk",
			gcc_blsp1_qup6_i2c_apps_clk, "f9928000.i2c"),

	CLK_LOOKUP_OF("wcnss_debug", wcnss_m_clk, "fb000000.qcom,wcnss-wlan"),

};

static struct clk_lookup msm_clocks_gcc_8974_only[] = {
	/* Camera Sensor clocks */
	CLK_LOOKUP_OF("cam_src_clk", gp1_clk_src, "2.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", gcc_gp1_clk, "2.qcom,camera"),
};

static struct clk *qup_i2c_clks[][2] = {
	{&gcc_blsp1_qup1_i2c_apps_clk.c, &blsp1_qup1_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup2_i2c_apps_clk.c, &blsp1_qup2_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup3_i2c_apps_clk.c, &blsp1_qup3_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup4_i2c_apps_clk.c, &blsp1_qup4_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup5_i2c_apps_clk.c, &blsp1_qup5_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup6_i2c_apps_clk.c, &blsp1_qup6_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup1_i2c_apps_clk.c, &blsp2_qup1_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup2_i2c_apps_clk.c, &blsp2_qup2_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup3_i2c_apps_clk.c, &blsp2_qup3_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup4_i2c_apps_clk.c, &blsp2_qup4_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup5_i2c_apps_clk.c, &blsp2_qup5_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup6_i2c_apps_clk.c, &blsp2_qup6_i2c_apps_clk_src.c,},
};

/* v1 to v2 clock changes */
void msm8974_v2_clock_override(void)
{
	int i;

	/* The parent of each of the QUP I2C clocks is an RCG on V2 */
	for (i = 0; i < ARRAY_SIZE(qup_i2c_clks); i++)
		qup_i2c_clks[i][0]->parent =  qup_i2c_clks[i][1];
}

/* v2 to Pro clock changes */
void msm8974_pro_clock_override(bool ac)
{
	ce1_clk_src.c.fmax[VDD_DIG_LOW] = 75000000;
	ce1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 150000000;
	ce1_clk_src.freq_tbl = ftbl_gcc_ce1_pro_clk;
	ce2_clk_src.c.fmax[VDD_DIG_LOW] = 75000000;
	ce2_clk_src.c.fmax[VDD_DIG_NOMINAL] = 150000000;
	ce2_clk_src.freq_tbl = ftbl_gcc_ce2_pro_clk;

	sdcc1_apps_clk_src.c.fmax[VDD_DIG_LOW] = 200000000;
	sdcc1_apps_clk_src.c.fmax[VDD_DIG_NOMINAL] = 400000000;
	sdcc1_apps_clk_src.freq_tbl = ftbl_gcc_sdcc1_apps_clk_pro;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-8974" },
	{ .compatible = "qcom,gcc-8974v2" },
	{ .compatible = "qcom,gcc-8974pro" },
	{ .compatible = "qcom,gcc-8974pro-ac" },
	{}
};

static int msm_gcc_8974_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *cxo_gcc;
	u32 regval;
	int ret;
	const char *compat = NULL;
	bool v2_compat = false, pro_compat = false, pro_ac_compat = false;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;
	pro_ac_compat = !strcmp(compat, "qcom,gcc-8974pro-ac");
	pro_compat = pro_ac_compat || !strcmp(compat, "qcom,gcc-8974pro");
	v2_compat = pro_compat || !strcmp(compat, "qcom,gcc-8974v2");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base.\n");
		return -ENOMEM;
	}

	virt_bases[GCC_BASE] = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!virt_bases[GCC_BASE]) {
		dev_err(&pdev->dev, "Failed to map in CC registers.\n");
		return -ENOMEM;
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the vdd_dig regulator!");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	cxo_gcc = cxo_clk_src.c.parent = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(cxo_gcc)) {
		if (!(PTR_ERR(cxo_gcc) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the the cxo clock!");
		return PTR_ERR(cxo_gcc);
	}

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE_REG));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE_REG));

	/*
	 * V2,Pro require additional votes to allow the LPASS and MMSS
	 * controllers to use GPLL0.
	 */
	if (v2_compat) {
		regval = readl_relaxed(
				GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
		writel_relaxed(regval | BIT(26) | BIT(25),
				GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
		gcc_usb30_mock_utmi_clk.max_div = 3;
		msm8974_v2_clock_override();
	}

	if (pro_compat)
		msm8974_pro_clock_override(pro_ac_compat);

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_gcc_8974,
				 ARRAY_SIZE(msm_clocks_gcc_8974));
	if (ret)
		return ret;

	if (pro_compat) {
		ret = of_msm_clock_register(pdev->dev.of_node,
				 msm_clocks_gcc_8974pro_only,
				 ARRAY_SIZE(msm_clocks_gcc_8974pro_only));
		if (ret)
			return ret;
	} else {
		ret = of_msm_clock_register(pdev->dev.of_node,
				 msm_clocks_gcc_8974_only,
				 ARRAY_SIZE(msm_clocks_gcc_8974_only));
		if (ret)
			return ret;
	}

	/*
	 * TODO: Temporarily enable NOC configuration AHB clocks. Remove when
	 * the bus driver is ready.
	 */
	clk_prepare_enable(&gcc_mmss_noc_cfg_ahb_clk.c);
	clk_prepare_enable(&gcc_ocmem_noc_cfg_ahb_clk.c);

	/* Set rates for single-rate clocks. */
	clk_set_rate(&usb30_master_clk_src.c,
			usb30_master_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&tsif_ref_clk_src.c,
			tsif_ref_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hs_system_clk_src.c,
			usb_hs_system_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_clk_src.c,
			usb_hsic_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_io_cal_clk_src.c,
			usb_hsic_io_cal_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_system_clk_src.c,
			usb_hsic_system_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb30_mock_utmi_clk_src.c,
			usb30_mock_utmi_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&pdm2_clk_src.c, pdm2_clk_src.freq_tbl[0].freq_hz);

	dev_info(&pdev->dev, "Registered GCC clocks.\n");

	return 0;
}

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_8974_probe,
	.driver = {
		.name = "qcom,gcc-8974",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_gcc_8974_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}
arch_initcall(msm_gcc_8974_init);

static struct clk_lookup msm_clocks_measure_8974[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "fb000000.qcom,wcnss-wlan"),
};

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-8974" },
	{}
};

static int msm_clock_debug_8974_probe(struct platform_device *pdev)
{
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	gcc_debug_mux_ops = mux_reg_ops;
	gcc_debug_mux_ops.set_mux_sel = gcc_set_mux_sel;

	mmss_debug_clk.c.parent = clk_get(&pdev->dev, "mmss_debug_mux");
	if (IS_ERR(mmss_debug_clk.c.parent)) {
		dev_err(&pdev->dev, "Failed to get MMSS debug mux\n");
		return PTR_ERR(mmss_debug_clk.c.parent);
	}

	kpss_debug_clk.c.parent = clk_get(&pdev->dev, "kpss_debug_mux");
	if (IS_ERR(kpss_debug_clk.c.parent)) {
		dev_err(&pdev->dev, "Failed to get KPSS debug mux\n");
		return PTR_ERR(kpss_debug_clk.c.parent);
	}

	rpm_debug_clk.c.parent = clk_get(&pdev->dev, "rpm_debug_mux");
	if (IS_ERR(rpm_debug_clk.c.parent)) {
		dev_err(&pdev->dev, "Failed to get RPM debug mux\n");
		return PTR_ERR(rpm_debug_clk.c.parent);
	}

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_measure_8974,
				 ARRAY_SIZE(msm_clocks_measure_8974));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered debug mux.\n");
	return ret;
}

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_8974_probe,
	.driver = {
		.name = "qcom,cc-debug-8974",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_clock_debug_8974_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}
late_initcall(msm_clock_debug_8974_init);
