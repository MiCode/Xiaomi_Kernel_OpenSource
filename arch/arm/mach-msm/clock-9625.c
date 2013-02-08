/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
	APCS_BASE,
	APCS_PLL_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))
#define LPASS_REG_BASE(x) (void __iomem *)(virt_bases[LPASS_BASE] + (x))
#define APCS_REG_BASE(x) (void __iomem *)(virt_bases[APCS_BASE] + (x))
#define APCS_PLL_REG_BASE(x) (void __iomem *)(virt_bases[APCS_PLL_BASE] + (x))

/* GCC registers */
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

#define GCC_DEBUG_CLK_CTL_REG          0x1880
#define CLOCK_FRQ_MEASURE_CTL_REG      0x1884
#define CLOCK_FRQ_MEASURE_STATUS_REG   0x1888
#define GCC_PLLTEST_PAD_CFG_REG        0x188C
#define GCC_XO_DIV4_CBCR_REG           0x10C8
#define APCS_GPLL_ENA_VOTE_REG         0x1480
#define APCS_CLOCK_BRANCH_ENA_VOTE     0x1484
#define APCS_CLOCK_SLEEP_ENA_VOTE      0x1488

#define APCS_CLK_DIAG_REG              0x001C

#define APCS_CPU_PLL_MODE_REG          0x0000
#define APCS_CPU_PLL_L_REG             0x0004
#define APCS_CPU_PLL_M_REG             0x0008
#define APCS_CPU_PLL_N_REG             0x000C
#define APCS_CPU_PLL_USER_CTL_REG      0x0010
#define APCS_CPU_PLL_CONFIG_CTL_REG    0x0014
#define APCS_CPU_PLL_TEST_CTL_REG      0x0018
#define APCS_CPU_PLL_STATUS_REG        0x001C

#define USB_HSIC_SYSTEM_CMD_RCGR       0x041C
#define USB_HSIC_XCVR_FS_CMD_RCGR      0x0424
#define USB_HSIC_CMD_RCGR              0x0440
#define USB_HSIC_IO_CAL_CMD_RCGR       0x0458
#define USB_HS_SYSTEM_CMD_RCGR         0x0490
#define SDCC2_APPS_CMD_RCGR            0x0510
#define SDCC3_APPS_CMD_RCGR            0x0550
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
#define PDM2_CMD_RCGR                  0x0CD0
#define CE1_CMD_RCGR                   0x1050
#define GP1_CMD_RCGR                   0x1904
#define GP2_CMD_RCGR                   0x1944
#define GP3_CMD_RCGR                   0x1984
#define QPIC_CMD_RCGR                  0x1A50
#define IPA_CMD_RCGR                   0x1A90

#define USB_HS_HSIC_BCR           0x0400
#define USB_HS_BCR                0x0480
#define SDCC2_BCR                 0x0500
#define SDCC3_BCR                 0x0540
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
#define PDM_BCR                   0x0CC0
#define PRNG_BCR                  0x0D00
#define BAM_DMA_BCR               0x0D40
#define BOOT_ROM_BCR              0x0E00
#define CE1_BCR                   0x1040
#define QPIC_BCR                  0x1040
#define IPA_BCR                   0x1A80


#define SYS_NOC_IPA_AXI_CBCR                     0x0128
#define USB_HSIC_AHB_CBCR                        0x0408
#define USB_HSIC_SYSTEM_CBCR                     0x040C
#define USB_HSIC_CBCR                            0x0410
#define USB_HSIC_IO_CAL_CBCR                     0x0414
#define USB_HSIC_XCVR_FS_CBCR                    0x042C
#define USB_HS_SYSTEM_CBCR                       0x0484
#define USB_HS_AHB_CBCR                          0x0488
#define SDCC2_APPS_CBCR                          0x0504
#define SDCC2_AHB_CBCR                           0x0508
#define SDCC3_APPS_CBCR                          0x0544
#define SDCC3_AHB_CBCR                           0x0548
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
#define BOOT_ROM_AHB_CBCR                        0x0E04
#define PDM_AHB_CBCR                             0x0CC4
#define PDM_XO4_CBCR                             0x0CC8
#define PDM_AHB_CBCR                             0x0CC4
#define PDM_XO4_CBCR                             0x0CC8
#define PDM2_CBCR                                0x0CCC
#define PRNG_AHB_CBCR                            0x0D04
#define BAM_DMA_AHB_CBCR                         0x0D44
#define MSG_RAM_AHB_CBCR                         0x0E44
#define CE1_CBCR                                 0x1044
#define CE1_AXI_CBCR                             0x1048
#define CE1_AHB_CBCR                             0x104C
#define GCC_AHB_CBCR                             0x10C0
#define GP1_CBCR                                 0x1900
#define GP2_CBCR                                 0x1940
#define GP3_CBCR                                 0x1980
#define QPIC_CBCR				 0x1A44
#define QPIC_AHB_CBCR                            0x1A48
#define IPA_CBCR                                 0x1A84
#define IPA_CNOC_CBCR                            0x1A88
#define IPA_SLEEP_CBCR                           0x1A8C

/* LPASS registers */
/* TODO: Needs to double check lpass regiserts after get the SWI for hw */
#define LPAPLL_MODE_REG				0x0000
#define LPAPLL_L_REG				0x0004
#define LPAPLL_M_REG				0x0008
#define LPAPLL_N_REG				0x000C
#define LPAPLL_USER_CTL_REG			0x0010
#define LPAPLL_CONFIG_CTL_REG			0x0014
#define LPAPLL_TEST_CTL_REG			0x0018
#define LPAPLL_STATUS_REG			0x001C

#define LPASS_DEBUG_CLK_CTL_REG			0x29000
#define LPASS_LPA_PLL_VOTE_APPS_REG		0x2000

#define LPAIF_PRI_CMD_RCGR			0xB000
#define LPAIF_SEC_CMD_RCGR			0xC000
#define LPAIF_PCM0_CMD_RCGR			0xF000
#define LPAIF_PCM1_CMD_RCGR			0x10000
#define SLIMBUS_CMD_RCGR			0x12000
#define LPAIF_PCMOE_CMD_RCGR			0x13000

#define AUDIO_CORE_BCR				0x4000

#define AUDIO_CORE_GDSCR			0x7000
#define AUDIO_CORE_LPAIF_PRI_OSR_CBCR		0xB014
#define AUDIO_CORE_LPAIF_PRI_IBIT_CBCR		0xB018
#define AUDIO_CORE_LPAIF_PRI_EBIT_CBCR		0xB01C
#define AUDIO_CORE_LPAIF_SEC_OSR_CBCR		0xC014
#define AUDIO_CORE_LPAIF_SEC_IBIT_CBCR		0xC018
#define AUDIO_CORE_LPAIF_SEC_EBIT_CBCR		0xC01C
#define AUDIO_CORE_LPAIF_PCM0_IBIT_CBCR		0xF014
#define AUDIO_CORE_LPAIF_PCM0_EBIT_CBCR		0xF018
#define AUDIO_CORE_LPAIF_PCM1_IBIT_CBCR		0x10014
#define AUDIO_CORE_LPAIF_PCM1_EBIT_CBCR		0x10018
#define AUDIO_CORE_RESAMPLER_CORE_CBCR		0x11014
#define AUDIO_CORE_RESAMPLER_LFABIF_CBCR	0x11018
#define AUDIO_CORE_SLIMBUS_CORE_CBCR		0x12014
#define AUDIO_CORE_SLIMBUS_LFABIF_CBCR		0x12018
#define AUDIO_CORE_LPAIF_PCM_DATA_OE_CBCR	0x13014

/* Mux source select values */
#define cxo_source_val	0
#define gpll0_source_val 1
#define gpll1_hsic_source_val 4
#define gnd_source_val	5
#define cxo_lpass_source_val 0
#define lpapll0_lpass_source_val 1
#define gpll0_lpass_source_val 5

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

#define F_LPASS(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_lpass_source_val), \
	}

#define F_APCS_PLL(f, l, m, n, pre_div, post_div, vco) \
	{ \
		.freq_hz = (f), \
		.l_val = (l), \
		.m_val = (m), \
		.n_val = (n), \
		.pre_div_val = BVAL(14, 12, (pre_div)), \
		.post_div_val = BVAL(9, 8, (post_div)), \
		.vco_val = BVAL(21, 20, (vco)), \
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

static const int vdd_corner[] = {
	[VDD_DIG_NONE]	  = RPM_REGULATOR_CORNER_NONE,
	[VDD_DIG_LOW]	  = RPM_REGULATOR_CORNER_SVS_SOC,
	[VDD_DIG_NOMINAL] = RPM_REGULATOR_CORNER_NORMAL,
	[VDD_DIG_HIGH]	  = RPM_REGULATOR_CORNER_SUPER_TURBO,
};

static struct regulator *vdd_dig_reg;

int set_vdd_dig(struct clk_vdd_class *vdd_class, int level)
{
	return regulator_set_voltage(vdd_dig_reg, vdd_corner[level],
					RPM_REGULATOR_CORNER_SUPER_TURBO);
}

static DEFINE_VDD_CLASS(vdd_dig, set_vdd_dig, VDD_DIG_NUM);

/* TODO: Needs to confirm the below values */
#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_BUS_CLK_TYPE	0x316b6c63
#define RPM_MEM_CLK_TYPE	0x326b6c63

#define RPM_SMD_KEY_ENABLE	0x62616E45

#define CXO_ID			0x0
#define QDSS_ID			0x1

#define PNOC_ID		0x0
#define SNOC_ID		0x1
#define CNOC_ID		0x2

#define BIMC_ID		0x0

#define D0_ID		 1
#define D1_ID		 2
#define A0_ID		 3
#define A1_ID		 4
#define A2_ID		 5

DEFINE_CLK_RPM_SMD_BRANCH(cxo_clk_src, cxo_a_clk_src,
				RPM_MISC_CLK_TYPE, CXO_ID, 19200000);

DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_ID, NULL);

DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_ID, NULL);

DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_d0, cxo_d0_a, D0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_d1, cxo_d1_a, D1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a0, cxo_a0_a, A0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a1, cxo_a1_a, A1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a2, cxo_a2_a, A2_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_d0_pin, cxo_d0_a_pin, D0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_d1_pin, cxo_d1_a_pin, D1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a0_pin, cxo_a0_a_pin, A0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a1_pin, cxo_a1_a_pin, A1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a2_pin, cxo_a2_a_pin, A2_ID);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
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

static struct pll_vote_clk gpll0_activeonly_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.status_reg = (void __iomem *)GPLL0_STATUS_REG,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 600000000,
		.dbg_name = "gpll0_activeonly_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_activeonly_clk_src.c),
	},
};

static struct pll_vote_clk lpapll0_clk_src = {
	.en_reg = (void __iomem *)LPASS_LPA_PLL_VOTE_APPS_REG,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)LPAPLL_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.rate = 393216000,
		.dbg_name = "lpapll0_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(lpapll0_clk_src.c),
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

static struct pll_freq_tbl apcs_pll_freq[] = {
	F_APCS_PLL(748800000, 0x27, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(998400000, 0x34, 0x0, 0x1, 0x0, 0x0, 0x0),
	PLL_F_END
};

/*
 * Need to skip handoff of the acpu pll to avoid handoff code
 * to turn off the pll when the acpu is running off this pll.
 */
static struct pll_clk apcspll_clk_src = {
	.mode_reg = (void __iomem *)APCS_CPU_PLL_MODE_REG,
	.l_reg = (void __iomem *)APCS_CPU_PLL_L_REG,
	.m_reg = (void __iomem *)APCS_CPU_PLL_M_REG,
	.n_reg = (void __iomem *)APCS_CPU_PLL_N_REG,
	.config_reg = (void __iomem *)APCS_CPU_PLL_USER_CTL_REG,
	.status_reg = (void __iomem *)APCS_CPU_PLL_STATUS_REG,
	.freq_tbl = apcs_pll_freq,
	.masks = {
		.vco_mask = BM(21, 20),
		.pre_div_mask = BM(14, 12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
	},
	.base = &virt_bases[APCS_PLL_BASE],
	.c = {
		.dbg_name = "apcspll_clk_src",
		.ops = &clk_ops_local_pll,
		CLK_INIT(apcspll_clk_src.c),
		.flags = CLKFLAG_SKIP_HANDOFF,
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

static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc_clk.c, LONG_MAX);

static struct clk_freq_tbl ftbl_gcc_ipa_clk[] = {
	F( 50000000,    gpll0,   12,   0,   0),
	F( 92310000,    gpll0,  6.5,   0,   0),
	F(100000000,    gpll0,    6,   0,   0),
	F_END
};

static struct rcg_clk ipa_clk_src = {
	.cmd_rcgr_reg =  IPA_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_ipa_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ipa_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(ipa_clk_src.c)
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_i2c_apps_clk[] = {
	F(19200000, cxo,    1, 0, 0),
	F(50000000, gpll0, 12, 0, 0),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_spi_apps_clk[] = {
	F(  960000,     cxo,   10,   1,   2),
	F( 4800000,     cxo,    4,   0,   0),
	F( 9600000,     cxo,    2,   0,   0),
	F(15000000,   gpll0,   10,   1,   4),
	F(19200000,     cxo,    1,   0,   0),
	F(25000000,   gpll0,   12,   1,   2),
	F(50000000,   gpll0,   12,   0,   0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c)
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_uart1_6_apps_clk[] = {
	F_GCC_GND,
	F( 3686400,    gpll0,    1,    96,   15625),
	F( 7372800,    gpll0,    1,   192,   15625),
	F(14745600,    gpll0,    1,   384,   15625),
	F(16000000,    gpll0,    5,     2,      15),
	F(19200000,      cxo,    1,     0,       0),
	F(24000000,    gpll0,    5,     1,       5),
	F(32000000,    gpll0,    1,     4,      75),
	F(40000000,    gpll0,   15,     0,       0),
	F(46400000,    gpll0,    1,    29,     375),
	F(48000000,    gpll0,  12.5,    0,       0),
	F(51200000,    gpll0,    1,    32,     375),
	F(56000000,    gpll0,    1,     7,      75),
	F(58982400,    gpll0,    1,  1536,   15625),
	F(60000000,    gpll0,   10,     0,       0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_uart5_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart5_apps_clk_src.c)
	},
};

static struct rcg_clk blsp1_uart6_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart6_apps_clk_src.c)
	},
};

static struct clk_freq_tbl ftbl_gcc_ce1_clk[] = {
	F( 50000000,    gpll0,   12,   0,   0),
	F(100000000,    gpll0,    6,   0,   0),
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

static struct clk_freq_tbl ftbl_gcc_gp_clk[] = {
	F(19200000,   cxo,   1,   0,   0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg =  GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c)
	},
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg =  GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c)
	},
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg =  GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c)
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F(60000000,   gpll0,  10,   0,   0),
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

static struct clk_freq_tbl ftbl_gcc_qpic_clk[] = {
	F( 50000000,    gpll0,   12,   0,   0),
	F(100000000,    gpll0,    6,   0,   0),
	F_END
};

static struct rcg_clk qpic_clk_src = {
	.cmd_rcgr_reg =  QPIC_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_qpic_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "qpic_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(qpic_clk_src.c)
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc2_apps_clk[] = {
	F(   144000,      cxo,   16,   3,   25),
	F(   400000,      cxo,   12,   1,    4),
	F( 20000000,    gpll0,   15,   1,    2),
	F( 25000000,    gpll0,   12,   1,    2),
	F( 50000000,    gpll0,   12,   0,    0),
	F(100000000,    gpll0,    6,   0,    0),
	F(200000000,    gpll0,    3,   0,    0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_sdcc3_apps_clk[] = {
	F(   144000,      cxo,   16,   3,   25),
	F(   400000,      cxo,   12,   1,    4),
	F( 20000000,    gpll0,   15,   1,    2),
	F( 25000000,    gpll0,   12,   1,    2),
	F( 50000000,    gpll0,   12,   0,    0),
	F(100000000,    gpll0,    6,   0,    0),
	F_END
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg =  SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c)
	},
};

static struct rcg_clk sdcc3_apps_clk_src = {
	.cmd_rcgr_reg =  SDCC3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc3_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc3_apps_clk_src.c)
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F(75000000,   gpll0,   8,   0,   0),
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
	F_HSIC(480000000,   gpll1,   1,   0,   0),
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
	F(9600000,   cxo,   2,   0,   0),
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
	F(75000000,   gpll0,   8,   0,   0),
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
	F(60000000,   gpll0,   10,   0,   0),
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
	.has_sibling = 0,
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
	.has_sibling = 0,
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
	.has_sibling = 0,
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

static struct branch_clk gcc_blsp1_uart5_apps_clk = {
	.cbcr_reg = BLSP1_UART5_APPS_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
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

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gp3_clk_src.c,
		.dbg_name = "gcc_gp3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_ipa_clk = {
	.cbcr_reg = IPA_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ipa_clk_src.c,
		.dbg_name = "gcc_ipa_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ipa_clk.c),
	},
};

static struct branch_clk gcc_ipa_cnoc_clk = {
	.cbcr_reg = IPA_CNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ipa_cnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ipa_cnoc_clk.c),
	},
};

static struct branch_clk gcc_ipa_sleep_clk = {
	.cbcr_reg = IPA_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ipa_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ipa_sleep_clk.c),
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

static struct branch_clk gcc_qpic_ahb_clk = {
	.cbcr_reg = QPIC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_qpic_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_qpic_ahb_clk.c),
	},
};

static struct branch_clk gcc_qpic_clk = {
	.cbcr_reg = QPIC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &qpic_clk_src.c,
		.dbg_name = "gcc_qpic_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_qpic_clk.c),
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc3_apps_clk_src.c,
		.dbg_name = "gcc_sdcc3_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_apps_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_ipa_axi_clk = {
	.cbcr_reg = SYS_NOC_IPA_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ipa_clk_src.c,
		.dbg_name = "gcc_sys_noc_ipa_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_ipa_axi_clk.c),
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
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_system_clk_src.c,
		.dbg_name = "gcc_usb_hsic_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_system_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_xcvr_fs_clk = {
	.cbcr_reg = USB_HSIC_XCVR_FS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_xcvr_fs_clk_src.c,
		.dbg_name = "gcc_usb_hsic_xcvr_fs_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_xcvr_fs_clk.c),
	},
};

/* LPASS clock data */
static struct clk_freq_tbl ftbl_audio_core_lpaif_clock[] = {
	F_LPASS(  512000,   lpapll0,   16,   1,   48),
	F_LPASS(  768000,   lpapll0,   16,   1,   32),
	F_LPASS( 1024000,   lpapll0,   16,   1,   24),
	F_LPASS( 1536000,   lpapll0,   16,   1,   16),
	F_LPASS( 2048000,   lpapll0,   16,   1,   12),
	F_LPASS( 3072000,   lpapll0,   16,   1,    8),
	F_LPASS( 4096000,   lpapll0,   16,   1,    6),
	F_LPASS( 6144000,   lpapll0,   16,   1,    4),
	F_LPASS( 8192000,   lpapll0,   16,   1,    3),
	F_LPASS(12288000,   lpapll0,   16,   1,    2),
	F_END
};

static struct clk_freq_tbl ftbl_audio_core_lpaif_pcm_clock[] = {
	F_LPASS(  512000,   lpapll0,   16,   1,   48),
	F_LPASS(  768000,   lpapll0,   16,   1,   32),
	F_LPASS( 1024000,   lpapll0,   16,   1,   24),
	F_LPASS( 1536000,   lpapll0,   16,   1,   16),
	F_LPASS( 2048000,   lpapll0,   16,   1,   12),
	F_LPASS( 3072000,   lpapll0,   16,   1,    8),
	F_LPASS( 4096000,   lpapll0,   16,   1,    6),
	F_LPASS( 6144000,   lpapll0,   16,   1,    4),
	F_LPASS( 8192000,   lpapll0,   16,   1,    3),
	F_END
};

static struct rcg_clk audio_core_lpaif_pcmoe_clk_src = {
	.cmd_rcgr_reg =  LPAIF_PCMOE_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_audio_core_lpaif_clock,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_pcmoe_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 12288000),
		CLK_INIT(audio_core_lpaif_pcmoe_clk_src.c)
	},
};

static struct rcg_clk audio_core_lpaif_pri_clk_src = {
	.cmd_rcgr_reg =  LPAIF_PRI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_audio_core_lpaif_clock,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_pri_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 12288000, NOMINAL, 24576000),
		CLK_INIT(audio_core_lpaif_pri_clk_src.c)
	},
};

static struct rcg_clk audio_core_lpaif_sec_clk_src = {
	.cmd_rcgr_reg =  LPAIF_SEC_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_audio_core_lpaif_clock,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_sec_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 12288000, NOMINAL, 24576000),
		CLK_INIT(audio_core_lpaif_sec_clk_src.c)
	},
};

static struct clk_freq_tbl ftbl_audio_core_slimbus_core_clock[] = {
	F_LPASS(26041000,   lpapll0,   1,   10,   151),
	F_END
};

static struct rcg_clk audio_core_slimbus_core_clk_src = {
	.cmd_rcgr_reg =  SLIMBUS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_audio_core_slimbus_core_clock,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_slimbus_core_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 13107000, NOMINAL, 26214000),
		CLK_INIT(audio_core_slimbus_core_clk_src.c)
	},
};

static struct rcg_clk audio_core_lpaif_pcm0_clk_src = {
	.cmd_rcgr_reg =  LPAIF_PCM0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_audio_core_lpaif_pcm_clock,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_pcm0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 4096000, NOMINAL, 8192000),
		CLK_INIT(audio_core_lpaif_pcm0_clk_src.c)
	},
};

static struct rcg_clk audio_core_lpaif_pcm1_clk_src = {
	.cmd_rcgr_reg =  LPAIF_PCM1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_audio_core_lpaif_pcm_clock,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_pcm1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 4096000, NOMINAL, 8192000),
		CLK_INIT(audio_core_lpaif_pcm1_clk_src.c)
	},
};

static struct branch_clk audio_core_slimbus_lfabif_clk = {
	.cbcr_reg = AUDIO_CORE_SLIMBUS_LFABIF_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_slimbus_lfabif_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_slimbus_lfabif_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pcm_data_oe_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PCM_DATA_OE_CBCR,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_lpaif_pcmoe_clk_src.c,
		.dbg_name = "audio_core_lpaif_pcm_data_oe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pcm_data_oe_clk.c),
	},
};

static struct branch_clk audio_core_slimbus_core_clk = {
	.cbcr_reg = AUDIO_CORE_SLIMBUS_CORE_CBCR,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_slimbus_core_clk_src.c,
		.dbg_name = "audio_core_slimbus_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_slimbus_core_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pri_ebit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PRI_EBIT_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_pri_ebit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pri_ebit_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pri_ibit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PRI_IBIT_CBCR,
	.has_sibling = 1,
	.max_div = 15,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_lpaif_pri_clk_src.c,
		.dbg_name = "audio_core_lpaif_pri_ibit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pri_ibit_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pri_osr_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PRI_OSR_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_lpaif_pri_clk_src.c,
		.dbg_name = "audio_core_lpaif_pri_osr_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pri_osr_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pcm0_ebit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PCM0_EBIT_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_pcm0_ebit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pcm0_ebit_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pcm0_ibit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PCM0_IBIT_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_lpaif_pcm0_clk_src.c,
		.dbg_name = "audio_core_lpaif_pcm0_ibit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pcm0_ibit_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_sec_ebit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_SEC_EBIT_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_sec_ebit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_sec_ebit_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_sec_ibit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_SEC_IBIT_CBCR,
	.has_sibling = 1,
	.max_div = 15,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_lpaif_sec_clk_src.c,
		.dbg_name = "audio_core_lpaif_sec_ibit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_sec_ibit_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_sec_osr_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_SEC_OSR_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_lpaif_sec_clk_src.c,
		.dbg_name = "audio_core_lpaif_sec_osr_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_sec_osr_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pcm1_ebit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PCM1_EBIT_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "audio_core_lpaif_pcm1_ebit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pcm1_ebit_clk.c),
	},
};

static struct branch_clk audio_core_lpaif_pcm1_ibit_clk = {
	.cbcr_reg = AUDIO_CORE_LPAIF_PCM1_IBIT_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.parent = &audio_core_lpaif_pcm1_clk_src.c,
		.dbg_name = "audio_core_lpaif_pcm1_ibit_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(audio_core_lpaif_pcm1_ibit_clk.c),
	},
};

static DEFINE_CLK_MEASURE(a5_m_clk);

#ifdef CONFIG_DEBUG_FS

struct measure_mux_entry {
	struct clk *c;
	int base;
	u32 debug_mux;
};

struct measure_mux_entry measure_mux[] = {
	{&gcc_pdm_ahb_clk.c,			GCC_BASE, 0x00d0},
	{&gcc_usb_hsic_xcvr_fs_clk.c,		GCC_BASE, 0x005d},
	{&gcc_usb_hsic_system_clk.c,		GCC_BASE, 0x0059},
	{&gcc_usb_hsic_io_cal_clk.c,		GCC_BASE, 0x005b},
	{&gcc_sdcc3_ahb_clk.c,			GCC_BASE, 0x0079},
	{&gcc_blsp1_qup5_i2c_apps_clk.c,	GCC_BASE, 0x009d},
	{&gcc_blsp1_qup1_spi_apps_clk.c,	GCC_BASE, 0x008a},
	{&gcc_blsp1_uart2_apps_clk.c,		GCC_BASE, 0x0091},
	{&gcc_blsp1_qup4_spi_apps_clk.c,	GCC_BASE, 0x0098},
	{&gcc_blsp1_qup3_spi_apps_clk.c,	GCC_BASE, 0x0093},
	{&gcc_blsp1_qup6_i2c_apps_clk.c,	GCC_BASE, 0x00a2},
	{&gcc_bam_dma_ahb_clk.c,		GCC_BASE, 0x00e0},
	{&gcc_sdcc3_apps_clk.c,			GCC_BASE, 0x0078},
	{&gcc_usb_hs_system_clk.c,		GCC_BASE, 0x0060},
	{&gcc_blsp1_ahb_clk.c,			GCC_BASE, 0x0088},
	{&gcc_blsp1_uart4_apps_clk.c,		GCC_BASE, 0x009a},
	{&gcc_blsp1_qup2_spi_apps_clk.c,	GCC_BASE, 0x008e},
	{&gcc_usb_hsic_ahb_clk.c,		GCC_BASE, 0x0058},
	{&gcc_blsp1_uart3_apps_clk.c,		GCC_BASE, 0x0095},
	{&gcc_ce1_axi_clk.c,			GCC_BASE, 0x0139},
	{&gcc_blsp1_qup5_spi_apps_clk.c,	GCC_BASE, 0x009c},
	{&gcc_usb_hs_ahb_clk.c,			GCC_BASE, 0x0061},
	{&gcc_blsp1_qup6_spi_apps_clk.c,	GCC_BASE, 0x00a1},
	{&gcc_prng_ahb_clk.c,			GCC_BASE, 0x00d8},
	{&gcc_blsp1_qup3_i2c_apps_clk.c,	GCC_BASE, 0x0094},
	{&gcc_usb_hsic_clk.c,			GCC_BASE, 0x005a},
	{&gcc_blsp1_uart6_apps_clk.c,		GCC_BASE, 0x00a3},
	{&gcc_sdcc2_apps_clk.c,			GCC_BASE, 0x0070},
	{&gcc_blsp1_uart1_apps_clk.c,		GCC_BASE, 0x008c},
	{&gcc_blsp1_qup4_i2c_apps_clk.c,	GCC_BASE, 0x0099},
	{&gcc_boot_rom_ahb_clk.c,		GCC_BASE, 0x00f8},
	{&gcc_ce1_ahb_clk.c,			GCC_BASE, 0x013a},
	{&gcc_pdm2_clk.c,			GCC_BASE, 0x00d2},
	{&gcc_blsp1_uart5_apps_clk.c,		GCC_BASE, 0x009e},
	{&gcc_blsp1_qup2_i2c_apps_clk.c,	GCC_BASE, 0x0090},
	{&gcc_blsp1_qup1_i2c_apps_clk.c,	GCC_BASE, 0x008b},
	{&gcc_sdcc2_ahb_clk.c,			GCC_BASE, 0x0071},
	{&gcc_ce1_clk.c,			GCC_BASE, 0x0138},
	{&gcc_sys_noc_ipa_axi_clk.c,		GCC_BASE, 0x0007},
	{&gcc_ipa_clk.c,			GCC_BASE, 0x01E0},
	{&gcc_ipa_cnoc_clk.c,			GCC_BASE, 0x01E1},
	{&gcc_ipa_sleep_clk.c,			GCC_BASE, 0x01E2},
	{&gcc_qpic_clk.c,			GCC_BASE, 0x01D8},
	{&gcc_qpic_ahb_clk.c,			GCC_BASE, 0x01D9},

	{&audio_core_lpaif_pcm_data_oe_clk.c,	LPASS_BASE, 0x0030},
	{&audio_core_slimbus_core_clk.c,	LPASS_BASE, 0x003d},
	{&audio_core_lpaif_pri_clk_src.c,	LPASS_BASE, 0x0017},
	{&audio_core_lpaif_sec_clk_src.c,	LPASS_BASE, 0x0016},
	{&audio_core_slimbus_core_clk_src.c,	LPASS_BASE, 0x0011},
	{&audio_core_lpaif_pcm1_clk_src.c,	LPASS_BASE, 0x0012},
	{&audio_core_lpaif_pcm0_clk_src.c,	LPASS_BASE, 0x0013},
	{&audio_core_lpaif_pcmoe_clk_src.c,	LPASS_BASE, 0x000f},
	{&audio_core_slimbus_lfabif_clk.c,	LPASS_BASE, 0x003e},

	{&a5_m_clk,				APCS_BASE, 0x3},

	{&dummy_clk,				N_BASES,    0x0000},
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

	writel_relaxed(0, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL_REG));
	writel_relaxed(0, GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));

	switch (measure_mux[i].base) {

	case GCC_BASE:
		clk_sel = measure_mux[i].debug_mux;
		break;

	case LPASS_BASE:
		clk_sel = 0x161;
		regval = BVAL(15, 0, measure_mux[i].debug_mux);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL_REG));

		/* Activate debug clock output */
		regval |= BIT(20);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL_REG));
		break;

	case APCS_BASE:
		clk_sel = 0x16A;
		regval = BVAL(5, 3, measure_mux[i].debug_mux);
		writel_relaxed(regval, APCS_REG_BASE(APCS_CLK_DIAG_REG));

		/* Activate debug clock output */
		regval |= BIT(7);
		writel_relaxed(regval, APCS_REG_BASE(APCS_CLK_DIAG_REG));
		break;

	default:
		return -EINVAL;
	}

	/* Set debug mux clock index */
	regval = BVAL(8, 0, clk_sel);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));

	/* Activate debug clock output */
	regval |= BIT(16);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));

	/* Make sure test vector is set before starting measurements. */
	mb();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL_REG));

	/* Wait for timer to become ready. */
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS_REG)) &
			BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(20)|ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL_REG));
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS_REG)) &
			BIT(25)) == 0)
		cpu_relax();

	/* Return measured ticks. */
	return readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS_REG)) &
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

	ret = clk_prepare_enable(&cxo_clk_src.c);
	if (ret) {
		pr_warning("CXO clock failed to enable. Can't measure\n");
		return 0;
	}

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch. */
	gcc_xo4_reg_backup = readl_relaxed(GCC_REG_BASE(GCC_XO_DIV4_CBCR_REG));
	writel_relaxed(0x1, GCC_REG_BASE(GCC_XO_DIV4_CBCR_REG));

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

	writel_relaxed(gcc_xo4_reg_backup, GCC_REG_BASE(GCC_XO_DIV4_CBCR_REG));

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short) {
		ret = 0;
	} else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((clk->sample_ticks * 10) + 35));
		ret = (raw_count_full * clk->multiplier);
	}

	writel_relaxed(0x51A00, GCC_REG_BASE(GCC_PLLTEST_PAD_CFG_REG));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(&cxo_clk_src.c);

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

static struct clk_lookup msm_clocks_9625[] = {
	CLK_LOOKUP("xo",	cxo_clk_src.c,	""),
	CLK_LOOKUP("measure",	measure_clk.c,	"debug"),

	CLK_LOOKUP("pll0", gpll0_activeonly_clk_src.c, "f9010008.qcom,acpuclk"),
	CLK_LOOKUP("pll14", apcspll_clk_src.c, "f9010008.qcom,acpuclk"),

	CLK_LOOKUP("dma_bam_pclk", gcc_bam_dma_ahb_clk.c, "msm_sps"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9924000.spi"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9925000.i2c"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f991d000.uart"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup1_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup1_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_spi_apps_clk.c, "f9924000.spi"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup3_i2c_apps_clk.c, "f9925000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup3_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup4_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup4_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup5_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup5_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup6_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup6_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart1_apps_clk.c, "f991d000.uart"),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart2_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart3_apps_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart4_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart5_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart6_apps_clk.c, ""),

	CLK_LOOKUP("core_clk_src", ce1_clk_src.c, ""),
	CLK_LOOKUP("core_clk", gcc_ce1_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_ce1_ahb_clk.c, ""),
	CLK_LOOKUP("bus_clk", gcc_ce1_axi_clk.c, ""),

	CLK_LOOKUP("core_clk", gcc_gp1_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_gp2_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_gp3_clk.c, ""),

	CLK_LOOKUP("iface_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng"),
	CLK_LOOKUP("core_src_clk", ipa_clk_src.c, "fd4c0000.qcom,ipa"),
	CLK_LOOKUP("core_clk", gcc_ipa_clk.c, "fd4c0000.qcom,ipa"),
	CLK_LOOKUP("bus_clk",  gcc_sys_noc_ipa_axi_clk.c, "fd4c0000.qcom,ipa"),
	CLK_LOOKUP("iface_clk",  gcc_ipa_cnoc_clk.c, "fd4c0000.qcom,ipa"),
	CLK_LOOKUP("inactivity_clk",  gcc_ipa_sleep_clk.c, "fd4c0000.qcom,ipa"),

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

	CLK_LOOKUP("core_clk", gcc_ce1_clk.c, "fd400000.qcom,qcedev"),
	CLK_LOOKUP("iface_clk", gcc_ce1_ahb_clk.c, "fd400000.qcom,qcedev"),
	CLK_LOOKUP("bus_clk", gcc_ce1_axi_clk.c, "fd400000.qcom,qcedev"),
	CLK_LOOKUP("core_clk_src", ce1_clk_src.c, "fd400000.qcom,qcedev"),

	CLK_LOOKUP("core_clk", gcc_ce1_clk.c, "fd400000.qcom,qcrypto"),
	CLK_LOOKUP("iface_clk", gcc_ce1_ahb_clk.c, "fd400000.qcom,qcrypto"),
	CLK_LOOKUP("bus_clk", gcc_ce1_axi_clk.c, "fd400000.qcom,qcrypto"),
	CLK_LOOKUP("core_clk_src", ce1_clk_src.c, "fd400000.qcom,qcrypto"),

	/* LPASS clocks */
	CLK_LOOKUP("core_clk", audio_core_slimbus_core_clk.c, "fe12f000.slim"),
	CLK_LOOKUP("iface_clk", audio_core_slimbus_lfabif_clk.c, ""),

	CLK_LOOKUP("core_clk", audio_core_lpaif_pri_clk_src.c,
		   "msm-dai-q6-mi2s.0"),
	CLK_LOOKUP("osr_clk", audio_core_lpaif_pri_osr_clk.c,
		   "msm-dai-q6-mi2s.0"),
	CLK_LOOKUP("ebit_clk", audio_core_lpaif_pri_ebit_clk.c,
		   "msm-dai-q6-mi2s.0"),
	CLK_LOOKUP("ibit_clk", audio_core_lpaif_pri_ibit_clk.c,
		   "msm-dai-q6-mi2s.0"),
	CLK_LOOKUP("core_clk", audio_core_lpaif_sec_clk_src.c,
		   "msm-dai-q6-mi2s.1"),
	CLK_LOOKUP("osr_clk", audio_core_lpaif_sec_osr_clk.c,
		   "msm-dai-q6-mi2s.1"),
	CLK_LOOKUP("ebit_clk", audio_core_lpaif_sec_ebit_clk.c,
		   "msm-dai-q6-mi2s.1"),
	CLK_LOOKUP("ibit_clk", audio_core_lpaif_sec_ibit_clk.c,
		   "msm-dai-q6-mi2s.1"),
	CLK_LOOKUP("core_clk", audio_core_lpaif_pcm0_clk_src.c, ""),
	CLK_LOOKUP("ebit_clk", audio_core_lpaif_pcm0_ebit_clk.c, ""),
	CLK_LOOKUP("ibit_clk", audio_core_lpaif_pcm0_ibit_clk.c, ""),
	CLK_LOOKUP("core_clk", audio_core_lpaif_pcm1_clk_src.c, ""),
	CLK_LOOKUP("ebit_clk", audio_core_lpaif_pcm1_ebit_clk.c, ""),
	CLK_LOOKUP("ibit_clk", audio_core_lpaif_pcm1_ibit_clk.c, ""),
	CLK_LOOKUP("core_oe_src_clk", audio_core_lpaif_pcmoe_clk_src.c, ""),
	CLK_LOOKUP("core_oe_clk", audio_core_lpaif_pcm_data_oe_clk.c, ""),

	/* RPM and voter clocks */
	CLK_LOOKUP("bus_clk", snoc_clk.c, ""),
	CLK_LOOKUP("bus_clk", pnoc_clk.c, ""),
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

	CLK_LOOKUP("dfab_clk", pnoc_sps_clk.c, "msm_sps"),

	CLK_LOOKUP("a5_m_clk", a5_m_clk, ""),

	/* CoreSight clocks */
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc322000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc318000.tpiu"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc31c000.replicator"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc307000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc31b000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc319000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc31a000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc321000.stm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc332000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc332000.jtagmm"),

	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc322000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc318000.tpiu"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc31c000.replicator"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc307000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc31b000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc319000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc31a000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc321000.stm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc332000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc332000.jtagmm"),

};

static struct pll_config_regs gpll0_regs __initdata = {
	.l_reg = (void __iomem *)GPLL0_L_REG,
	.m_reg = (void __iomem *)GPLL0_M_REG,
	.n_reg = (void __iomem *)GPLL0_N_REG,
	.config_reg = (void __iomem *)GPLL0_USER_CTL_REG,
	.mode_reg = (void __iomem *)GPLL0_MODE_REG,
	.base = &virt_bases[GCC_BASE],
};

/* GPLL0 at 600 MHz, main output enabled. */
static struct pll_config gpll0_config __initdata = {
	.l = 0x1f,
	.m = 0x1,
	.n = 0x4,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs gpll1_regs __initdata = {
	.l_reg = (void __iomem *)GPLL1_L_REG,
	.m_reg = (void __iomem *)GPLL1_M_REG,
	.n_reg = (void __iomem *)GPLL1_N_REG,
	.config_reg = (void __iomem *)GPLL1_USER_CTL_REG,
	.mode_reg = (void __iomem *)GPLL1_MODE_REG,
	.base = &virt_bases[GCC_BASE],
};

/* GPLL1 at 480 MHz, main output enabled. */
static struct pll_config gpll1_config __initdata = {
	.l = 0x19,
	.m = 0x0,
	.n = 0x1,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs lpapll0_regs __initdata = {
	.l_reg = (void __iomem *)LPAPLL_L_REG,
	.m_reg = (void __iomem *)LPAPLL_M_REG,
	.n_reg = (void __iomem *)LPAPLL_N_REG,
	.config_reg = (void __iomem *)LPAPLL_USER_CTL_REG,
	.mode_reg = (void __iomem *)LPAPLL_MODE_REG,
	.base = &virt_bases[LPASS_BASE],
};

/* LPAPLL0 at 393.216 MHz, main output enabled. */
static struct pll_config lpapll0_config __initdata = {
	.l = 0x28,
	.m = 0x18,
	.n = 0x19,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = BVAL(9, 8, 0x1),
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

#define PLL_AUX_OUTPUT_BIT 1
#define PLL_AUX2_OUTPUT_BIT 2

/*
 * TODO: Need to remove this function when the v2 hardware
 * fix the broken lock status bit.
 */
#define PLL_OUTCTRL BIT(0)
#define PLL_BYPASSNL BIT(1)
#define PLL_RESET_N BIT(2)

static DEFINE_SPINLOCK(sr_pll_reg_lock);

static int sr_pll_clk_enable_9625(struct clk *c)
{
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(c);
	u32 mode;
	void __iomem *mode_reg = *pll->base + (u32)pll->mode_reg;

	spin_lock_irqsave(&sr_pll_reg_lock, flags);

	/* Disable PLL bypass mode and de-assert reset. */
	mode = readl_relaxed(mode_reg);
	mode |= PLL_BYPASSNL | PLL_RESET_N;
	writel_relaxed(mode, mode_reg);

	/* Wait for pll to lock. */
	udelay(100);

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, mode_reg);

	/* Ensure the write above goes through before returning. */
	mb();

	spin_unlock_irqrestore(&sr_pll_reg_lock, flags);
	return 0;
}

static void __init configure_apcs_pll(void)
{
	u32 regval;

	clk_set_rate(&apcspll_clk_src.c, 998400000);

	writel_relaxed(0x00141200,
			APCS_PLL_REG_BASE(APCS_CPU_PLL_CONFIG_CTL_REG));

	/* Enable AUX and AUX2 output */
	regval = readl_relaxed(APCS_PLL_REG_BASE(APCS_CPU_PLL_USER_CTL_REG));
	regval |= BIT(PLL_AUX_OUTPUT_BIT) | BIT(PLL_AUX2_OUTPUT_BIT);
	writel_relaxed(regval, APCS_PLL_REG_BASE(APCS_CPU_PLL_USER_CTL_REG));
}

#define PWR_ON_MASK		BIT(31)
#define EN_REST_WAIT_MASK	(0xF << 20)
#define EN_FEW_WAIT_MASK	(0xF << 16)
#define CLK_DIS_WAIT_MASK	(0xF << 12)
#define SW_OVERRIDE_MASK	BIT(2)
#define HW_CONTROL_MASK		BIT(1)
#define SW_COLLAPSE_MASK	BIT(0)

/* Wait 2^n CXO cycles between all states. Here, n=2 (4 cycles). */
#define EN_REST_WAIT_VAL	(0x2 << 20)
#define EN_FEW_WAIT_VAL		(0x2 << 16)
#define CLK_DIS_WAIT_VAL	(0x2 << 12)
#define GDSC_TIMEOUT_US		50000

static void __init reg_init(void)
{
	u32 regval, status;
	int ret;

	if (!(readl_relaxed(GCC_REG_BASE(GPLL0_STATUS_REG))
			& gpll0_clk_src.status_mask))
		configure_sr_hpm_lp_pll(&gpll0_config, &gpll0_regs, 1);

	if (!(readl_relaxed(GCC_REG_BASE(GPLL1_STATUS_REG))
			& gpll1_clk_src.status_mask))
		configure_sr_hpm_lp_pll(&gpll1_config, &gpll1_regs, 1);

	configure_sr_hpm_lp_pll(&lpapll0_config, &lpapll0_regs, 1);

	/* TODO: Remove A5 pll configuration once the bootloader is avaiable */
	regval = readl_relaxed(APCS_PLL_REG_BASE(APCS_CPU_PLL_MODE_REG));
	if ((regval & BM(2, 0)) != 0x7)
		configure_apcs_pll();

	/* TODO:
	 * 1) do we need to turn on AUX2 output too?
	 * 2) if need to vote off all sleep clocks
	 */

	/* Enable GPLL0's aux outputs. */
	regval = readl_relaxed(GCC_REG_BASE(GPLL0_USER_CTL_REG));
	regval |= BIT(PLL_AUX_OUTPUT_BIT) | BIT(PLL_AUX2_OUTPUT_BIT);
	writel_relaxed(regval, GCC_REG_BASE(GPLL0_USER_CTL_REG));

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE_REG));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE_REG));

	/*
	 * TODO: Confirm that no clocks need to be voted on in this sleep vote
	 * register.
	 */
	writel_relaxed(0x0, GCC_REG_BASE(APCS_CLOCK_SLEEP_ENA_VOTE));

	/*
	 * TODO: The following sequence enables the LPASS audio core GDSC.
	 * Remove when this becomes unnecessary.
	 */

	/*
	 * Disable HW trigger: collapse/restore occur based on registers writes.
	 * Disable SW override: Use hardware state-machine for sequencing.
	 */
	regval = readl_relaxed(LPASS_REG_BASE(AUDIO_CORE_GDSCR));
	regval &= ~(HW_CONTROL_MASK | SW_OVERRIDE_MASK);

	/* Configure wait time between states. */
	regval &= ~(EN_REST_WAIT_MASK | EN_FEW_WAIT_MASK | CLK_DIS_WAIT_MASK);
	regval |= EN_REST_WAIT_VAL | EN_FEW_WAIT_VAL | CLK_DIS_WAIT_VAL;
	writel_relaxed(regval, LPASS_REG_BASE(AUDIO_CORE_GDSCR));

	regval = readl_relaxed(LPASS_REG_BASE(AUDIO_CORE_GDSCR));
	regval &= ~BIT(0);
	writel_relaxed(regval, LPASS_REG_BASE(AUDIO_CORE_GDSCR));

	ret = readl_poll_timeout(LPASS_REG_BASE(AUDIO_CORE_GDSCR), status,
				status & PWR_ON_MASK, 50, GDSC_TIMEOUT_US);
	WARN(ret, "LPASS Audio Core GDSC did not power on.\n");
}

static void __init msm9625_clock_post_init(void)
{
	/*
	 * Hold an active set vote for CXO; this is because CXO is expected
	 * to remain on whenever CPUs aren't power collapsed.
	 */
	clk_prepare_enable(&cxo_a_clk_src.c);

	/*
	 * TODO: This call is to prevent sending 0Hz to rpm to turn off pnoc.
	 * Needs to remove this after vote of pnoc from sdcc driver is ready.
	 */
	clk_prepare_enable(&pnoc_msmbus_a_clk.c);

	/* Set rates for single-rate clocks. */
	clk_set_rate(&usb_hs_system_clk_src.c,
			usb_hs_system_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_clk_src.c,
			usb_hsic_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_io_cal_clk_src.c,
			usb_hsic_io_cal_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_system_clk_src.c,
			usb_hsic_system_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_xcvr_fs_clk_src.c,
			usb_hsic_xcvr_fs_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&pdm2_clk_src.c, pdm2_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&audio_core_slimbus_core_clk_src.c,
			audio_core_slimbus_core_clk_src.freq_tbl[0].freq_hz);
	/*
	 * TODO: set rate on behalf of the i2c driver until the i2c driver
	 *	 distinguish v1/v2 and call set rate accordingly.
	 */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2)
		clk_set_rate(&blsp1_qup3_i2c_apps_clk_src.c,
			blsp1_qup3_i2c_apps_clk_src.freq_tbl[0].freq_hz);
}

#define GCC_CC_PHYS		0xFC400000
#define GCC_CC_SIZE		SZ_16K

#define LPASS_CC_PHYS		0xFE000000
#define LPASS_CC_SIZE		SZ_256K

#define APCS_GCC_CC_PHYS	0xF9011000
#define APCS_GCC_CC_SIZE	SZ_4K

#define APCS_PLL_PHYS		0xF9008018
#define APCS_PLL_SIZE		0x18

static struct clk *i2c_apps_clks[][2] __initdata = {
	{&gcc_blsp1_qup1_i2c_apps_clk.c, &blsp1_qup1_i2c_apps_clk_src.c},
	{&gcc_blsp1_qup2_i2c_apps_clk.c, &blsp1_qup2_i2c_apps_clk_src.c},
	{&gcc_blsp1_qup3_i2c_apps_clk.c, &blsp1_qup3_i2c_apps_clk_src.c},
	{&gcc_blsp1_qup4_i2c_apps_clk.c, &blsp1_qup4_i2c_apps_clk_src.c},
	{&gcc_blsp1_qup5_i2c_apps_clk.c, &blsp1_qup5_i2c_apps_clk_src.c},
	{&gcc_blsp1_qup6_i2c_apps_clk.c, &blsp1_qup6_i2c_apps_clk_src.c},
};

static void __init msm9625_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-9625: Unable to ioremap GCC memory!");

	virt_bases[LPASS_BASE] = ioremap(LPASS_CC_PHYS, LPASS_CC_SIZE);
	if (!virt_bases[LPASS_BASE])
		panic("clock-9625: Unable to ioremap LPASS_CC memory!");

	virt_bases[APCS_BASE] = ioremap(APCS_GCC_CC_PHYS, APCS_GCC_CC_SIZE);
	if (!virt_bases[APCS_BASE])
		panic("clock-9625: Unable to ioremap APCS_GCC_CC memory!");

	virt_bases[APCS_PLL_BASE] = ioremap(APCS_PLL_PHYS, APCS_PLL_SIZE);
	if (!virt_bases[APCS_PLL_BASE])
		panic("clock-9625: Unable to ioremap APCS_PLL memory!");

	/* The parent of each of the QUP I2C APPS clocks is an RCG on v2 */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2) {
		int i, num_cores = ARRAY_SIZE(i2c_apps_clks);
		for (i = 0; i < num_cores; i++)
			i2c_apps_clks[i][0]->parent = i2c_apps_clks[i][1];
	}

	clk_ops_local_pll.enable = sr_pll_clk_enable_9625;

	vdd_dig_reg = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig_reg))
		panic("clock-9625: Unable to get the vdd_dig regulator!");

	vote_vdd_level(&vdd_dig, VDD_DIG_HIGH);
	regulator_enable(vdd_dig_reg);

	enable_rpm_scaling();

	reg_init();
}

static int __init msm9625_clock_late_init(void)
{
	return unvote_vdd_level(&vdd_dig, VDD_DIG_HIGH);
}

struct clock_init_data msm9625_clock_init_data __initdata = {
	.table = msm_clocks_9625,
	.size = ARRAY_SIZE(msm_clocks_9625),
	.pre_init = msm9625_clock_pre_init,
	.post_init = msm9625_clock_post_init,
	.late_init = msm9625_clock_late_init,
};
