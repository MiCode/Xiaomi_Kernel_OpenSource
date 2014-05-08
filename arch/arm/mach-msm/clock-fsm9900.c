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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-krait.h>

#include <soc/qcom/socinfo.h>
#include <soc/qcom/rpm-smd.h>

#include "clock.h"

enum {
	GCC_BASE,
	APCS_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))
#define APCS_REG_BASE(x) (void __iomem *)(virt_bases[APCS_BASE] + (x))

#define GPLL0_MODE                       0x0000
#define GPLL0_L                          0x0004
#define GPLL0_M                          0x0008
#define GPLL0_N                          0x000C
#define GPLL0_USER_CTL                   0x0010
#define GPLL0_CONFIG_CTL                 0x0014
#define GPLL0_TEST_CTL                   0x0018
#define GPLL0_STATUS                     0x001C
#define GPLL1_MODE                       0x0040
#define GPLL1_L                          0x0044
#define GPLL1_M                          0x0048
#define GPLL1_N                          0x004C
#define GPLL1_USER_CTL                   0x0050
#define GPLL1_CONFIG_CTL                 0x0054
#define GPLL1_TEST_CTL                   0x0058
#define GPLL1_STATUS                     0x005C
#define GPLL2_MODE                       0x0080
#define GPLL2_L                          0x0084
#define GPLL2_M                          0x0088
#define GPLL2_N                          0x008C
#define GPLL2_USER_CTL                   0x0090
#define GPLL2_CONFIG_CTL                 0x0094
#define GPLL2_TEST_CTL                   0x0098
#define GPLL2_STATUS                     0x009C
#define OCMEM_NOC_CFG_AHB_CBCR           0x0248
#define MMSS_NOC_CFG_AHB_CBCR            0x024C
#define MMSS_VPU_MAPLE_SYS_NOC_AXI_CBCR  0x026C
#define USB_HS_BCR                       0x0480
#define USB_HS_SYSTEM_CBCR               0x0484
#define USB_HS_AHB_CBCR                  0x0488
#define USB_HS_INACTIVITY_TIMERS_CBCR    0x048C
#define USB_HS_SYSTEM_CMD_RCGR           0x0490
#define USB2A_PHY_BCR                    0x04A8
#define USB2A_PHY_SLEEP_CBCR             0x04AC
#define SDCC1_BCR                        0x04C0
#define SDCC1_APPS_CMD_RCGR              0x04D0
#define SDCC1_APPS_CBCR                  0x04C4
#define SDCC1_AHB_CBCR                   0x04C8
#define SDCC2_BCR                        0x0500
#define SDCC2_APPS_CMD_RCGR              0x0510
#define SDCC2_APPS_CBCR                  0x0504
#define SDCC2_AHB_CBCR                   0x0508
#define SDCC2_INACTIVITY_TIMERS_CBCR     0x050C
#define BLSP1_BCR                        0x05C0
#define BLSP1_AHB_CBCR                   0x05C4
#define BLSP_UART_SIM_CMD_RCGR           0x0600
#define BLSP_UART_SIM_CFG_RCGR           0x0604
#define BLSP1_QUP1_BCR                   0x0640
#define BLSP1_QUP1_SPI_APPS_CBCR         0x0644
#define BLSP1_QUP1_I2C_APPS_CBCR         0x0648
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR     0x0660
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR     0x06E0
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR     0x0760
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR     0x07E0
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR     0x0860
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR     0x08E0
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR     0x09A0
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR     0x0A20
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR     0x0AA0
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR     0x0B20
#define BLSP2_QUP5_I2C_APPS_CMD_RCGR     0x0BA0
#define BLSP2_QUP6_I2C_APPS_CMD_RCGR     0x0C20
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR     0x064C
#define BLSP1_UART1_BCR                  0x0680
#define BLSP1_UART1_APPS_CBCR            0x0684
#define BLSP1_UART1_APPS_CMD_RCGR        0x068C
#define BLSP1_QUP2_BCR                   0x06C0
#define BLSP1_QUP2_SPI_APPS_CBCR         0x06C4
#define BLSP1_QUP2_I2C_APPS_CBCR         0x06C8
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR     0x06CC
#define BLSP1_UART2_BCR                  0x0700
#define BLSP1_UART2_APPS_CBCR            0x0704
#define BLSP1_UART2_APPS_CMD_RCGR        0x070C
#define BLSP1_QUP3_BCR                   0x0740
#define BLSP1_QUP3_SPI_APPS_CBCR         0x0744
#define BLSP1_QUP3_I2C_APPS_CBCR         0x0748
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR     0x074C
#define BLSP1_UART3_BCR                  0x0780
#define BLSP1_UART3_APPS_CBCR            0x0784
#define BLSP1_UART3_APPS_CMD_RCGR        0x078C
#define BLSP1_QUP4_BCR                   0x07C0
#define BLSP1_QUP4_SPI_APPS_CBCR         0x07C4
#define BLSP1_QUP4_I2C_APPS_CBCR         0x07C8
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR     0x07CC
#define BLSP1_UART4_BCR                  0x0800
#define BLSP1_UART4_APPS_CBCR            0x0804
#define BLSP1_UART4_APPS_CMD_RCGR        0x080C
#define BLSP1_QUP5_BCR                   0x0840
#define BLSP1_QUP5_SPI_APPS_CBCR         0x0844
#define BLSP1_QUP5_I2C_APPS_CBCR         0x0848
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR     0x084C
#define BLSP1_UART5_BCR                  0x0880
#define BLSP1_UART5_APPS_CBCR            0x0884
#define BLSP1_UART5_APPS_CMD_RCGR        0x088C
#define BLSP1_UART5_APPS_CFG_RCGR        0x0890
#define BLSP1_QUP6_BCR                   0x08C0
#define BLSP1_QUP6_SPI_APPS_CBCR         0x08C4
#define BLSP1_QUP6_I2C_APPS_CBCR         0x08C8
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR     0x08CC
#define BLSP1_UART6_BCR                  0x0900
#define BLSP1_UART6_APPS_CBCR            0x0904
#define BLSP1_UART6_APPS_CMD_RCGR        0x090C
#define BLSP2_BCR                        0x0940
#define BLSP2_AHB_CBCR                   0x0944
#define BLSP2_QUP1_BCR                   0x0980
#define BLSP2_QUP1_SPI_APPS_CBCR         0x0984
#define BLSP2_QUP1_I2C_APPS_CBCR         0x0988
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR     0x098C
#define BLSP2_UART1_BCR                  0x09C0
#define BLSP2_UART1_APPS_CBCR            0x09C4
#define BLSP2_UART1_APPS_CMD_RCGR        0x09CC
#define BLSP2_QUP2_BCR                   0x0A00
#define BLSP2_QUP2_SPI_APPS_CBCR         0x0A04
#define BLSP2_QUP2_I2C_APPS_CBCR         0x0A08
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR     0x0A0C
#define BLSP2_UART2_BCR                  0x0A40
#define BLSP2_UART2_APPS_CBCR            0x0A44
#define BLSP2_UART2_APPS_CMD_RCGR        0x0A4C
#define BLSP2_QUP3_BCR                   0x0A80
#define BLSP2_QUP3_SPI_APPS_CBCR         0x0A84
#define BLSP2_QUP3_I2C_APPS_CBCR         0x0A88
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR     0x0A8C
#define BLSP2_UART3_BCR                  0x0AC0
#define BLSP2_UART3_APPS_CBCR            0x0AC4
#define BLSP2_UART3_APPS_CMD_RCGR        0x0ACC
#define BLSP2_QUP4_BCR                   0x0B00
#define BLSP2_QUP4_SPI_APPS_CBCR         0x0B04
#define BLSP2_QUP4_I2C_APPS_CBCR         0x0B08
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR     0x0B0C
#define BLSP2_UART4_BCR                  0x0B40
#define BLSP2_UART4_APPS_CBCR            0x0B44
#define BLSP2_UART4_APPS_CMD_RCGR        0x0B4C
#define BLSP2_QUP5_BCR                   0x0B80
#define BLSP2_QUP5_SPI_APPS_CBCR         0x0B84
#define BLSP2_QUP5_I2C_APPS_CBCR         0x0B88
#define BLSP2_QUP5_SPI_APPS_CMD_RCGR     0x0B8C
#define BLSP2_UART5_BCR                  0x0BC0
#define BLSP2_UART5_APPS_CBCR            0x0BC4
#define BLSP2_UART5_APPS_CMD_RCGR        0x0BCC
#define BLSP2_QUP6_BCR                   0x0C00
#define BLSP2_QUP6_SPI_APPS_CBCR         0x0C04
#define BLSP2_QUP6_I2C_APPS_CBCR         0x0C08
#define BLSP2_QUP6_SPI_APPS_CMD_RCGR     0x0C0C
#define BLSP2_UART6_BCR                  0x0C40
#define BLSP2_UART6_APPS_CBCR            0x0C44
#define BLSP2_UART6_APPS_CMD_RCGR        0x0C4C
#define PDM_BCR                          0x0CC0
#define PDM_AHB_CBCR                     0x0CC4
#define PDM2_CBCR                        0x0CCC
#define PDM2_CMD_RCGR                    0x0CD0
#define PRNG_BCR                         0x0D00
#define PRNG_AHB_CBCR                    0x0D04
#define BAM_DMA_BCR                      0x0D40
#define BAM_DMA_AHB_CBCR                 0x0D44
#define BAM_DMA_INACTIVITY_TIMERS_CBCR   0x0D48
#define BOOT_ROM_AHB_CBCR                0x0E04
#define CE1_BCR                          0x1040
#define CE1_CMD_RCGR                     0x1050
#define CE1_CBCR                         0x1044
#define CE1_AXI_CBCR                     0x1048
#define CE1_AHB_CBCR                     0x104C
#define CE2_BCR                          0x1080
#define CE2_CMD_RCGR                     0x1090
#define CE2_CBCR                         0x1084
#define CE2_AXI_CBCR                     0x1088
#define CE2_AHB_CBCR                     0x108C
#define GCC_XO_DIV4_CBCR                 0x10C8
#define APCS_GPLL_ENA_VOTE               0x1480
#define APCS_CLOCK_BRANCH_ENA_VOTE       0x1484
#define APCS_CLOCK_SLEEP_ENA_VOTE        0x1488
#define GCC_DEBUG_CLK_CTL                0x1880
#define CLOCK_FRQ_MEASURE_CTL            0x1884
#define CLOCK_FRQ_MEASURE_STATUS         0x1888
#define GCC_PLLTEST_PAD_CFG              0x188C
#define GCC_GP1_CBCR                     0x1900
#define GCC_GP1_CMD_RCGR                 0x1904
#define GCC_GP2_CBCR                     0x1940
#define GCC_GP2_CMD_RCGR                 0x1944
#define GCC_GP3_CBCR                     0x1980
#define GCC_GP3_CMD_RCGR                 0x1984
#define GPLL4_MODE                       0x1DC0
#define GPLL4_L                          0x1DC4
#define GPLL4_M                          0x1DC8
#define GPLL4_N                          0x1DCC
#define GPLL4_USER_CTL                   0x1DD0
#define GPLL4_CONFIG_CTL                 0x1DD4
#define GPLL4_TEST_CTL                   0x1DD8
#define GPLL4_STATUS                     0x1DDC
#define MMPLL10_PLL_MODE                 0x2140
#define MMPLL10_PLL_L_VAL                0x2144
#define MMPLL10_PLL_M_VAL                0x2148
#define MMPLL10_PLL_N_VAL                0x214C
#define MMPLL10_PLL_USER_CTL             0x2150
#define MMPLL10_PLL_CONFIG_CTL           0x2154
#define MMPLL10_PLL_TEST_CTL             0x2158
#define MMPLL10_PLL_STATUS               0x215C
#define PCIE_0_BCR                       0x1AC0
#define PCIE_0_PHY_BCR                   0x1B00
#define PCIE_0_CFG_AHB_CBCR              0x1B0C
#define PCIE_0_PIPE_CBCR                 0x1B14
#define PCIE_0_SLV_AXI_CBCR              0x1B04
#define PCIE_0_AUX_CBCR                  0x1B10
#define PCIE_0_MSTR_AXI_CBCR             0x1B08
#define PCIE_0_PIPE_CMD_RCGR             0x1B18
#define PCIE_0_AUX_CMD_RCGR              0x1B2C
#define PCIE_1_BCR                       0x1B40
#define PCIE_1_PHY_BCR                   0x1B80
#define PCIE_1_CFG_AHB_CBCR              0x1B8C
#define PCIE_1_PIPE_CBCR                 0x1B94
#define PCIE_1_SLV_AXI_CBCR              0x1B84
#define PCIE_1_AUX_CBCR                  0x1B90
#define PCIE_1_MSTR_AXI_CBCR             0x1B88
#define PCIE_1_PIPE_CMD_RCGR             0x1B98
#define PCIE_1_AUX_CMD_RCGR              0x1BAC
#define CE3_BCR                          0x1D00
#define CE3_CMD_RCGR                     0x1D10
#define CE3_CBCR                         0x1D04
#define CE3_AXI_CBCR                     0x1D08
#define CE3_AHB_CBCR                     0x1D0C
#define PCIE_0_PHY_LDO_EN                0x1E00
#define PCIE_1_PHY_LDO_EN                0x1E04
#define EMAC_0_PHY_LDO_EN		 0x1E08
#define EMAC_1_PHY_LDO_EN		 0x1E0C
#define CE4_BCR                          0x2180
#define CE4_CMD_RCGR                     0x2190
#define CE4_CBCR                         0x2184
#define CE4_AXI_CBCR                     0x2188
#define CE4_AHB_CBCR                     0x218C

#define CE5_BCR                          0x21C0
#define CE5_CMD_RCGR                     0x21D0
#define CE5_CBCR                         0x21C4
#define CE5_AXI_CBCR                     0x21C8
#define CE5_AHB_CBCR                     0x21CC

#define CE6_BCR                          0x2200
#define CE6_CMD_RCGR                     0x2210
#define CE6_CBCR                         0x2204
#define CE6_AXI_CBCR                     0x2208
#define CE6_AHB_CBCR                     0x220C

#define CE7_BCR                          0x2240
#define CE7_CMD_RCGR                     0x2250
#define CE7_CBCR                         0x2244
#define CE7_AXI_CBCR                     0x2248
#define CE7_AHB_CBCR                     0x224C

#define CE8_BCR                          0x2280
#define CE8_CMD_RCGR                     0x2290
#define CE8_CBCR                         0x2284
#define CE8_AXI_CBCR                     0x2288
#define CE8_AHB_CBCR                     0x228C

#define SYS_NOC_EMAC_AHB_CBCR            0x2580
#define SYS_NOC_EMAC_AHB_CMD_RCGR        0x2584
#define SYS_NOC_EMAC_AHB_CFG_RCGR        0x2588
#define EMAC_0_BCR                       0x25C0
#define EMAC_0_AXI_CBCR                  0x25C4
#define EMAC_0_AHB_CBCR                  0x25C8
#define EMAC_0_SYS_25M_CBCR              0x25CC
#define EMAC_0_TX_CBCR                   0x25D0
#define EMAC_0_125M_CBCR                 0x25D4
#define EMAC_0_RX_CBCR                   0x25D8
#define EMAC_0_SYS_CBCR                  0x25DC
#define EMAC_0_SYS_25M_CMD_RCGR          0x25E0
#define EMAC_0_SYS_25M_CFG_RCGR          0x25E4
#define EMAC_0_TX_CMD_RCGR               0x2600
#define EMAC_0_TX_CFG_RCGR               0x2604
#define EMAC_0_TX_M                      0x2608
#define EMAC_0_TX_N                      0x260C
#define EMAC_0_TX_D                      0x2610
#define EMAC_0_125M_CMD_RCGR             0x2614
#define EMAC_0_125M_CFG_RCGR             0x2618
#define EMAC_1_BCR                       0x2640
#define EMAC_1_AXI_CBCR                  0x2644
#define EMAC_1_AHB_CBCR                  0x2648
#define EMAC_1_SYS_25M_CBCR              0x264C
#define EMAC_1_TX_CBCR                   0x2650
#define EMAC_1_125M_CBCR                 0x2654
#define EMAC_1_RX_CBCR                   0x2658
#define EMAC_1_SYS_CBCR                  0x265C
#define EMAC_1_SYS_25M_CMD_RCGR          0x2660
#define EMAC_1_SYS_25M_CFG_RCGR          0x2664
#define EMAC_1_TX_CMD_RCGR               0x2680
#define EMAC_1_TX_CFG_RCGR               0x2684
#define EMAC_1_TX_M                      0x2688
#define EMAC_1_TX_N                      0x268C
#define EMAC_1_TX_D                      0x2690
#define EMAC_1_125M_CMD_RCGR             0x2694
#define EMAC_1_125M_CFG_RCGR             0x2698
#define GLB_CLK_DIAG	                 0x001C
#define L2_CBCR				 0x004C

#define AHB_CMD_RCGR                     0x5000
#define MMSS_MMSSNOC_AHB_CBCR            0x5024
#define MMSS_MMSSNOC_BTO_AHB_CBCR        0x5028
#define MMSS_MISC_AHB_CBCR               0x502C
#define AXI_CMD_RCGR                     0x5040
#define MMSS_S0_AXI_CBCR                 0x5064
#define MMSS_MMSSNOC_AXI_CBCR            0x506C
#define OCMEMNOC_CMD_RCGR                0x5090
#define MMSS_DEBUG_CLK_CTL               0x0900


/* Mux source select values */
#define xo_source_val	0
#define gpll0_source_val 1
#define gpll1_source_val 2
#define gpll4_source_val 5
#define mmpll10_source_val 1
#define gnd_source_val	5
#define sdcc1_gnd_source_val 6
#define pcie_pipe_source_val 2
#define emac0_125m_source_val 1
#define emac0_tx_source_val 2

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

/* TODO RPM clocks are never modified in this chip */

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define CXO_ID			0x0

DEFINE_CLK_RPM_SMD_BRANCH(xo_clk_src, xo_a_clk_src,
				RPM_MISC_CLK_TYPE, CXO_ID, 19200000);
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

static struct pll_vote_clk gpll4_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GPLL4_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 288000000,
		.dbg_name = "gpll4_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll4_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk[] = {
	F( 19200000,         xo,    1, 0, 0),
	F( 50000000,      gpll0,   12, 0, 0),
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
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk[] = {
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
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
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
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
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_uart1_6_apps_clk[] = {
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
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
		CLK_INIT(blsp1_uart6_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup1_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup1_spi_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup2_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup2_spi_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup3_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup3_spi_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup4_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup4_spi_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup5_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup5_spi_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup6_i2c_apps_clk_src.c),
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
		CLK_INIT(blsp2_qup6_spi_apps_clk_src.c),
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
		CLK_INIT(blsp2_uart6_apps_clk_src.c),
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

static struct clk_freq_tbl ftbl_gcc_ce3_clk[] = {
	F( 50000000,      gpll0,   12, 0, 0),
	F( 85710000,      gpll0,    7, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(171430000,      gpll0,  3.5, 0, 0),
	F_END
};

static struct rcg_clk ce3_clk_src = {
	.cmd_rcgr_reg = CE3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_ce3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ce3_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(ce3_clk_src.c),
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

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(sdcc2_apps_clk_src.c),
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

static struct branch_clk gcc_blsp1_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup5_i2c_apps_clk_src.c,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup6_i2c_apps_clk_src.c,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup1_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup2_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup3_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup4_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup5_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup6_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
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
	.has_sibling = 0,
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
	.has_sibling = 0,
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
	.has_sibling = 0,
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
	.has_sibling = 0,
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
	.has_sibling = 0,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart6_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart6_apps_clk.c),
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

static struct branch_clk gcc_ce3_ahb_clk = {
	.cbcr_reg = CE3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ce3_ahb_clk.c),
	},
};

static struct branch_clk gcc_ce3_axi_clk = {
	.cbcr_reg = CE3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce3_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ce3_axi_clk.c),
	},
};

static struct branch_clk gcc_ce3_clk = {
	.cbcr_reg = CE3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ce3_clk_src.c,
		.dbg_name = "gcc_ce3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ce3_clk.c),
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc1_apps_clk_src.c,
		.dbg_name = "gcc_sdcc1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
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

static struct branch_clk gcc_emac0_axi_clk = {
	.cbcr_reg = EMAC_0_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac0_axi_clk.c),
	},
};

static struct branch_clk gcc_emac1_axi_clk = {
	.cbcr_reg = EMAC_1_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac1_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac1_axi_clk.c),
	},
};

static struct branch_clk gcc_emac0_ahb_clk = {
	.cbcr_reg = EMAC_0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac0_ahb_clk.c),
	},
};

static struct branch_clk gcc_emac1_ahb_clk = {
	.cbcr_reg = EMAC_1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac1_ahb_clk.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_emac0_1_125m_clk[] = {
	F(     19200000,              xo,    1, 0, 0),
	F_EXT( 125000000,      emac0_125m,   1, 0, 0),
	F_END
};

static struct rcg_clk emac0_125m_clk_src = {
	.cmd_rcgr_reg = EMAC_0_125M_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_emac0_1_125m_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac0_125m_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(emac0_125m_clk_src.c),
	},
};

static struct rcg_clk emac1_125m_clk_src = {
	.cmd_rcgr_reg = EMAC_1_125M_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_emac0_1_125m_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac1_125m_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(emac1_125m_clk_src.c),
	},
};

static struct branch_clk gcc_emac0_125m_clk = {
	.cbcr_reg = EMAC_0_125M_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &emac0_125m_clk_src.c,
		.dbg_name = "gcc_emac0_125m_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac0_125m_clk.c),
	},
};

static struct branch_clk gcc_emac1_125m_clk = {
	.cbcr_reg = EMAC_1_125M_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &emac1_125m_clk_src.c,
		.dbg_name = "gcc_emac1_125m_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac1_125m_clk.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_emac0_1_sys_25m_clk[] = {
	F(     19200000,              xo,   1, 0, 0),
	F_EXT( 25000000,      emac0_125m,   5, 0, 0),
	F_END
};

static struct rcg_clk emac0_sys_25m_clk_src = {
	.cmd_rcgr_reg = EMAC_0_SYS_25M_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_emac0_1_sys_25m_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac0_sys_25m_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(emac0_sys_25m_clk_src.c),
	},
};

static struct rcg_clk emac1_sys_25m_clk_src = {
	.cmd_rcgr_reg = EMAC_1_SYS_25M_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_emac0_1_sys_25m_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac1_sys_25m_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(emac1_sys_25m_clk_src.c),
	},
};

static struct branch_clk gcc_emac0_sys_25m_clk = {
	.cbcr_reg = EMAC_0_SYS_25M_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &emac0_sys_25m_clk_src.c,
		.dbg_name = "gcc_emac0_sys_25m_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac0_sys_25m_clk.c),
	},
};

static struct branch_clk gcc_emac1_sys_25m_clk = {
	.cbcr_reg = EMAC_1_SYS_25M_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &emac1_sys_25m_clk_src.c,
		.dbg_name = "gcc_emac1_sys_25m_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac1_sys_25m_clk.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_emac0_1_tx_clk[] = {
	F(     19200000,              xo,    1, 0, 0),
	F_EXT( 125000000,       emac0_tx,    1, 0, 0),
	F_END
};

static struct rcg_clk emac0_tx_clk_src = {
	.cmd_rcgr_reg = EMAC_0_TX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_emac0_1_tx_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac0_tx_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(emac0_tx_clk_src.c),
	},
};

static struct rcg_clk emac1_tx_clk_src = {
	.cmd_rcgr_reg = EMAC_1_TX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_emac0_1_tx_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "emac1_tx_clk_src",
		.ops = &clk_ops_rcg_mnd,
		CLK_INIT(emac1_tx_clk_src.c),
	},
};

static struct branch_clk gcc_emac0_tx_clk = {
	.cbcr_reg = EMAC_0_TX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &emac0_tx_clk_src.c,
		.dbg_name = "gcc_emac0_tx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac0_tx_clk.c),
	},
};

static struct branch_clk gcc_emac1_tx_clk = {
	.cbcr_reg = EMAC_1_TX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &emac1_tx_clk_src.c,
		.dbg_name = "gcc_emac1_tx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac1_tx_clk.c),
	},
};

static struct branch_clk gcc_emac0_rx_clk = {
	.cbcr_reg = EMAC_0_RX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac0_rx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac0_rx_clk.c),
	},
};

static struct branch_clk gcc_emac1_rx_clk = {
	.cbcr_reg = EMAC_1_RX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac1_rx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac1_rx_clk.c),
	},
};

static struct branch_clk gcc_emac0_sys_clk = {
	.cbcr_reg = EMAC_0_SYS_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac0_sys_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac0_sys_clk.c),
	},
};

static struct branch_clk gcc_emac1_sys_clk = {
	.cbcr_reg = EMAC_1_SYS_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_emac1_sys_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_emac1_sys_clk.c),
	},
};

static struct pll_clk mmpll10_clk_src = {
	.mode_reg = (void __iomem *)MMPLL10_PLL_MODE,
	.status_reg = (void __iomem *)MMPLL10_PLL_STATUS,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.dbg_name = "mmpll10_pll_clk_src",
		.rate = 345600000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll10_clk_src.c),
	},
};

static DEFINE_CLK_MEASURE(l2_m_clk);
static DEFINE_CLK_MEASURE(krait0_m_clk);
static DEFINE_CLK_MEASURE(krait1_m_clk);
static DEFINE_CLK_MEASURE(krait2_m_clk);
static DEFINE_CLK_MEASURE(krait3_m_clk);

#ifdef CONFIG_DEBUG_FS

struct measure_mux_entry {
	struct clk *c;
	int base;
	u32 debug_mux;
};

enum {
	M_ACPU0 = 0,
	M_ACPU1,
	M_ACPU2,
	M_ACPU3,
	M_L2,
};

struct measure_mux_entry measure_mux[] = {
	{&gcc_usb_hs_system_clk.c,		GCC_BASE, 0x0060},
	{&gcc_usb_hs_ahb_clk.c,			GCC_BASE, 0x0061},
	{&gcc_usb2a_phy_sleep_clk.c,		GCC_BASE, 0x0063},
	{&gcc_sdcc1_apps_clk.c,			GCC_BASE, 0x0068},
	{&gcc_sdcc1_ahb_clk.c,			GCC_BASE, 0x0069},
	{&gcc_sdcc2_apps_clk.c,			GCC_BASE, 0x0070},
	{&gcc_sdcc2_ahb_clk.c,			GCC_BASE, 0x0071},
	{&gcc_blsp1_ahb_clk.c,			GCC_BASE, 0x0088},
	{&gcc_blsp1_qup1_spi_apps_clk.c,	GCC_BASE, 0x008a},
	{&gcc_blsp1_qup1_i2c_apps_clk.c,	GCC_BASE, 0x008b},
	{&gcc_blsp1_uart1_apps_clk.c,		GCC_BASE, 0x008c},
	{&gcc_blsp1_qup2_spi_apps_clk.c,	GCC_BASE, 0x008e},
	{&gcc_blsp1_qup2_i2c_apps_clk.c,	GCC_BASE, 0x0090},
	{&gcc_blsp1_uart2_apps_clk.c,		GCC_BASE, 0x0091},
	{&gcc_blsp1_qup3_spi_apps_clk.c,	GCC_BASE, 0x0093},
	{&gcc_blsp1_qup3_i2c_apps_clk.c,	GCC_BASE, 0x0094},
	{&gcc_blsp1_uart3_apps_clk.c,		GCC_BASE, 0x0095},
	{&gcc_blsp1_qup4_spi_apps_clk.c,	GCC_BASE, 0x0098},
	{&gcc_blsp1_qup4_i2c_apps_clk.c,	GCC_BASE, 0x0099},
	{&gcc_blsp1_uart4_apps_clk.c,		GCC_BASE, 0x009a},
	{&gcc_blsp1_qup5_spi_apps_clk.c,	GCC_BASE, 0x009c},
	{&gcc_blsp1_qup5_i2c_apps_clk.c,	GCC_BASE, 0x009d},
	{&gcc_blsp1_uart5_apps_clk.c,		GCC_BASE, 0x009e},
	{&gcc_blsp1_qup6_spi_apps_clk.c,	GCC_BASE, 0x00a1},
	{&gcc_blsp1_qup6_i2c_apps_clk.c,	GCC_BASE, 0x00a2},
	{&gcc_blsp1_uart6_apps_clk.c,		GCC_BASE, 0x00a3},
	{&gcc_blsp2_ahb_clk.c,			GCC_BASE, 0x00a8},
	{&gcc_blsp2_qup1_spi_apps_clk.c,	GCC_BASE, 0x00aa},
	{&gcc_blsp2_qup1_i2c_apps_clk.c,	GCC_BASE, 0x00ab},
	{&gcc_blsp2_uart1_apps_clk.c,		GCC_BASE, 0x00ac},
	{&gcc_blsp2_qup2_spi_apps_clk.c,	GCC_BASE, 0x00ae},
	{&gcc_blsp2_qup2_i2c_apps_clk.c,	GCC_BASE, 0x00b0},
	{&gcc_blsp2_uart2_apps_clk.c,		GCC_BASE, 0x00b1},
	{&gcc_blsp2_qup3_spi_apps_clk.c,	GCC_BASE, 0x00b3},
	{&gcc_blsp2_qup3_i2c_apps_clk.c,	GCC_BASE, 0x00b4},
	{&gcc_blsp2_uart3_apps_clk.c,		GCC_BASE, 0x00b5},
	{&gcc_blsp2_qup4_spi_apps_clk.c,	GCC_BASE, 0x00b8},
	{&gcc_blsp2_qup4_i2c_apps_clk.c,	GCC_BASE, 0x00b9},
	{&gcc_blsp2_uart4_apps_clk.c,		GCC_BASE, 0x00ba},
	{&gcc_blsp2_qup5_spi_apps_clk.c,	GCC_BASE, 0x00bc},
	{&gcc_blsp2_qup5_i2c_apps_clk.c,	GCC_BASE, 0x00bd},
	{&gcc_blsp2_uart5_apps_clk.c,		GCC_BASE, 0x00be},
	{&gcc_blsp2_qup6_spi_apps_clk.c,	GCC_BASE, 0x00c1},
	{&gcc_blsp2_qup6_i2c_apps_clk.c,	GCC_BASE, 0x00c2},
	{&gcc_blsp2_uart6_apps_clk.c,		GCC_BASE, 0x00c3},
	{&gcc_pdm_ahb_clk.c,			GCC_BASE, 0x00d0},
	{&gcc_pdm2_clk.c,			GCC_BASE, 0x00d2},
	{&gcc_prng_ahb_clk.c,			GCC_BASE, 0x00d8},
	{&gcc_bam_dma_ahb_clk.c,		GCC_BASE, 0x00e0},
	{&gcc_boot_rom_ahb_clk.c,		GCC_BASE, 0x00f8},
	{&gcc_ce1_clk.c,			GCC_BASE, 0x0138},
	{&gcc_ce1_axi_clk.c,			GCC_BASE, 0x0139},
	{&gcc_ce1_ahb_clk.c,			GCC_BASE, 0x013a},
	{&gcc_ce2_clk.c,			GCC_BASE, 0x0140},
	{&gcc_ce2_axi_clk.c,			GCC_BASE, 0x0141},
	{&gcc_ce2_ahb_clk.c,			GCC_BASE, 0x0142},
	{&gcc_pcie_0_slv_axi_clk.c,		GCC_BASE, 0x01f8},
	{&gcc_pcie_0_mstr_axi_clk.c,		GCC_BASE, 0x01f9},
	{&gcc_pcie_0_cfg_ahb_clk.c,		GCC_BASE, 0x01fa},
	{&gcc_pcie_0_aux_clk.c,			GCC_BASE, 0x01fb},
	{&gcc_pcie_0_pipe_clk.c,		GCC_BASE, 0x01fc},
	{&gcc_pcie_1_slv_axi_clk.c,		GCC_BASE, 0x0200},
	{&gcc_pcie_1_mstr_axi_clk.c,		GCC_BASE, 0x0201},
	{&gcc_pcie_1_cfg_ahb_clk.c,		GCC_BASE, 0x0202},
	{&gcc_pcie_1_aux_clk.c,			GCC_BASE, 0x0203},
	{&gcc_pcie_1_pipe_clk.c,		GCC_BASE, 0x0204},
	{&gcc_ce3_clk.c,			GCC_BASE, 0x0228},
	{&gcc_ce3_axi_clk.c,			GCC_BASE, 0x0229},
	{&gcc_ce3_ahb_clk.c,			GCC_BASE, 0x022a},
	{&gcc_emac0_axi_clk.c,			GCC_BASE, 0x01a8},
	{&gcc_emac0_ahb_clk.c,			GCC_BASE, 0x01a9},
	{&gcc_emac0_sys_25m_clk.c,		GCC_BASE, 0x01aa},
	{&gcc_emac0_tx_clk.c,			GCC_BASE, 0x01ab},
	{&gcc_emac0_125m_clk.c,			GCC_BASE, 0x01ac},
	{&gcc_emac0_rx_clk.c,			GCC_BASE, 0x01ad},
	{&gcc_emac0_sys_clk.c,			GCC_BASE, 0x01ae},
	{&gcc_emac1_axi_clk.c,			GCC_BASE, 0x01b0},
	{&gcc_emac1_ahb_clk.c,			GCC_BASE, 0x01b1},
	{&gcc_emac1_sys_25m_clk.c,		GCC_BASE, 0x01b2},
	{&gcc_emac1_tx_clk.c,			GCC_BASE, 0x01b3},
	{&gcc_emac1_125m_clk.c,			GCC_BASE, 0x01b4},
	{&gcc_emac1_rx_clk.c,			GCC_BASE, 0x01b5},
	{&gcc_emac1_sys_clk.c,			GCC_BASE, 0x01b6},

	{&krait0_clk.c,				APCS_BASE, M_ACPU0},
	{&krait1_clk.c,				APCS_BASE, M_ACPU1},
	{&krait2_clk.c,				APCS_BASE, M_ACPU2},
	{&krait3_clk.c,				APCS_BASE, M_ACPU3},
	{&l2_clk.c,				APCS_BASE, M_L2},

	{&dummy_clk,				N_BASES, 0x0000},
};

/* TODO: Need to consider the new mux selection for pll test */
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

	switch (measure_mux[i].base) {

	case GCC_BASE:
		writel_relaxed(0, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));
		clk_sel = measure_mux[i].debug_mux;
		break;

	case APCS_BASE:
		clk->multiplier = 4;
		clk_sel = 0x16A;

		if (measure_mux[i].debug_mux == M_L2)
			regval = BIT(12);
		else
			regval = measure_mux[i].debug_mux << 8;

		writel_relaxed(BIT(0), APCS_REG_BASE(L2_CBCR));
		writel_relaxed(regval, APCS_REG_BASE(GLB_CLK_DIAG));
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

	ret = clk_prepare_enable(&xo_clk_src.c);
	if (ret) {
		pr_warn("CXO clock failed to enable. Can't measure\n");
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

	/*TODO: confirm if this value is correct. */
	writel_relaxed(0x51A00, GCC_REG_BASE(GCC_PLLTEST_PAD_CFG));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(&xo_clk_src.c);

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
		.flags = CLKFLAG_MEASURE,
		CLK_INIT(measure_clk.c),
	},
	.multiplier = 1,
};

static struct clk_lookup fsm_clocks_9900[] = {
	/* Dummy CE clocks are defined to satisfy the CE driver */
	CLK_DUMMY("core_clk",     NULL,         "fd440000.qcom,qcrypto", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fd440000.qcom,qcrypto", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fd440000.qcom,qcrypto", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fd440000.qcom,qcrypto", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe040000.qcom,qcrypto", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe040000.qcom,qcrypto", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe040000.qcom,qcrypto", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe040000.qcom,qcrypto", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe000000.qcom,qcrypto", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe000000.qcom,qcrypto", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe000000.qcom,qcrypto", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe000000.qcom,qcrypto", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe140000.qcom,qcota", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe140000.qcom,qcota", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe140000.qcom,qcota", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe140000.qcom,qcota", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe0c0000.qcom,qcota", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe0c0000.qcom,qcota", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe0c0000.qcom,qcota", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe0c0000.qcom,qcota", OFF),

	CLK_DUMMY("dma_bam_pclk", NULL,         "msm_sps",             OFF),
	CLK_DUMMY("dfab_clk",     NULL,         "msm_sps",             OFF),

	CLK_LOOKUP("measure",	measure_clk.c,	"debug"),
	CLK_LOOKUP("gpll0", gpll0_clk_src.c, ""),

	/* RCG source clocks */
	CLK_LOOKUP("",	sdcc1_apps_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc2_apps_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hs_system_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_0_aux_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_0_pipe_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_1_aux_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_1_pipe_clk_src.c,	""),

	/* BLSP1  clocks. Only the valid configs are present in the table */
	CLK_LOOKUP("iface_clk",	gcc_blsp1_ahb_clk.c,	"f991f000.serial"),
	CLK_LOOKUP("iface_clk",	gcc_blsp1_ahb_clk.c,	"f9924000.i2c"),
	CLK_LOOKUP("core_clk",	gcc_blsp1_uart3_apps_clk.c, "f991f000.serial"),
	CLK_LOOKUP("core_clk",	gcc_blsp1_qup2_i2c_apps_clk.c, "f9924000.i2c"),

	/* BLSP2  clocks. Only the valid configs are present in the table */
	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f995d000.qcom,uim"),
	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f9960000.serial"),
	CLK_LOOKUP("iface_clk",	gcc_blsp2_ahb_clk.c, "f9966000.i2c"),
	CLK_LOOKUP("core_clk",	gcc_blsp2_qup4_i2c_apps_clk.c, "f9966000.i2c"),
	CLK_LOOKUP("core_clk",	gcc_blsp2_uart1_apps_clk.c,
						"f995d000.qcom,uim"),
	CLK_LOOKUP("core_clk",	gcc_blsp2_uart4_apps_clk.c, "f9960000.serial"),

	/* BLSP SIM clock */
	CLK_LOOKUP("sim_clk", blsp_sim_clk_src.c, "f995d000.qcom,uim"),

	CLK_LOOKUP("iface_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng"),

	CLK_LOOKUP("",	gcc_boot_rom_ahb_clk.c,	""),

	CLK_LOOKUP("pdm2_clk",  gcc_pdm2_clk.c, "fd4a4090.qcom,rfic"),
	CLK_LOOKUP("ahb_clk",   gcc_pdm_ahb_clk.c, "fd4a4090.qcom,rfic"),

	/* SDCC clocks */
	CLK_LOOKUP("iface_clk",	gcc_sdcc1_ahb_clk.c,	   "msm_sdcc.1"),
	CLK_LOOKUP("core_clk",	gcc_sdcc1_apps_clk.c,	   "msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",	gcc_sdcc2_ahb_clk.c,	   "msm_sdcc.2"),
	CLK_LOOKUP("core_clk",	gcc_sdcc2_apps_clk.c,	   "msm_sdcc.2"),

	/* USB clocks */
	CLK_LOOKUP("iface_clk", gcc_usb_hs_ahb_clk.c,      "f9a55000.usb"),
	CLK_LOOKUP("core_clk",  gcc_usb_hs_system_clk.c,   "f9a55000.usb"),
	CLK_LOOKUP("sleep_clk",	gcc_usb2a_phy_sleep_clk.c, "f9a55000.usb"),
	CLK_LOOKUP("xo",        xo_usb_hs_host_clk.c,      "f9a55000.usb"),

	CLK_LOOKUP("iface_clk",	gcc_usb_hs_ahb_clk.c,	   "msm_ehci_host"),
	CLK_LOOKUP("core_clk",	gcc_usb_hs_system_clk.c,   "msm_ehci_host"),
	CLK_LOOKUP("sleep_clk",	gcc_usb2a_phy_sleep_clk.c, "msm_ehci_host"),
	CLK_LOOKUP("xo",	xo_usb_hs_host_clk.c,      "msm_ehci_host"),

	/* EMAC clocks */
	CLK_LOOKUP("axi_clk",	gcc_emac0_axi_clk.c,	 "feb20000.qcom,emac"),
	CLK_LOOKUP("cfg_ahb_clk", gcc_emac0_ahb_clk.c,	 "feb20000.qcom,emac"),
	CLK_LOOKUP("25m_clk",	emac0_sys_25m_clk_src.c, "feb20000.qcom,emac"),
	CLK_LOOKUP("125m_clk",	emac0_125m_clk_src.c,	 "feb20000.qcom,emac"),
	CLK_LOOKUP("tx_clk",	emac0_tx_clk_src.c,	 "feb20000.qcom,emac"),
	CLK_LOOKUP("rx_clk",	gcc_emac0_rx_clk.c,	 "feb20000.qcom,emac"),
	CLK_LOOKUP("sys_clk",	gcc_emac0_sys_clk.c,	 "feb20000.qcom,emac"),
	CLK_LOOKUP("axi_clk",	gcc_emac1_axi_clk.c,	 "feb00000.qcom,emac"),
	CLK_LOOKUP("cfg_ahb_clk", gcc_emac1_ahb_clk.c,	 "feb00000.qcom,emac"),
	CLK_LOOKUP("25m_clk",	emac1_sys_25m_clk_src.c, "feb00000.qcom,emac"),
	CLK_LOOKUP("125m_clk",	emac1_125m_clk_src.c,	 "feb00000.qcom,emac"),
	CLK_LOOKUP("tx_clk",	emac1_tx_clk_src.c,	 "feb00000.qcom,emac"),
	CLK_LOOKUP("rx_clk",	gcc_emac1_rx_clk.c,	 "feb00000.qcom,emac"),
	CLK_LOOKUP("sys_clk",	gcc_emac1_sys_clk.c,	 "feb00000.qcom,emac"),

	/* PCIE clocks */

	CLK_LOOKUP("pcie_0_aux_clk", gcc_pcie_0_aux_clk.c,
						"fc520000.qcom,pcie"),
	CLK_LOOKUP("pcie_0_cfg_ahb_clk", gcc_pcie_0_cfg_ahb_clk.c,
						"fc520000.qcom,pcie"),
	CLK_LOOKUP("pcie_0_mstr_axi_clk", gcc_pcie_0_mstr_axi_clk.c,
						"fc520000.qcom,pcie"),
	CLK_LOOKUP("pcie_0_pipe_clk", gcc_pcie_0_pipe_clk.c,
						"fc520000.qcom,pcie"),
	CLK_LOOKUP("pcie_0_slv_axi_clk", gcc_pcie_0_slv_axi_clk.c,
						"fc520000.qcom,pcie"),
	CLK_DUMMY("pcie_0_ref_clk_src", NULL, "fc520000.qcom,pcie", OFF),

	CLK_LOOKUP("pcie_1_aux_clk", gcc_pcie_1_aux_clk.c,
						"fc528000.qcom,pcie"),
	CLK_LOOKUP("pcie_1_cfg_ahb_clk", gcc_pcie_1_cfg_ahb_clk.c,
						"fc528000.qcom,pcie"),
	CLK_LOOKUP("pcie_1_mstr_axi_clk", gcc_pcie_1_mstr_axi_clk.c,
						"fc528000.qcom,pcie"),
	CLK_LOOKUP("pcie_1_pipe_clk", gcc_pcie_1_pipe_clk.c,
						"fc528000.qcom,pcie"),
	CLK_LOOKUP("pcie_1_slv_axi_clk", gcc_pcie_1_slv_axi_clk.c,
						"fc528000.qcom,pcie"),
	CLK_DUMMY("pcie_1_ref_clk_src", NULL, "fc528000.qcom,pcie", OFF),

	CLK_LOOKUP("hfpll_src", xo_a_clk_src.c,
					"f9016000.qcom,clock-krait"),
	CLK_LOOKUP("aux_clk",   gpll0_ao_clk_src.c,
					"f9016000.qcom,clock-krait"),
	CLK_LOOKUP("xo_clk",    xo_clk_src.c, ""),

	/* MPM */
	CLK_LOOKUP("xo", xo_clk_src.c, "fc4281d0.qcom,mpm"),

	/* LDO */
	CLK_LOOKUP("pcie_0_ldo",        pcie_0_phy_ldo.c, "fc520000.qcom,pcie"),
	CLK_LOOKUP("pcie_1_ldo",        pcie_1_phy_ldo.c, "fc528000.qcom,pcie"),

	/* QSEECOM clocks */
	CLK_LOOKUP("core_clk",     gcc_ce1_clk.c,         "qseecom"),
	CLK_LOOKUP("iface_clk",    gcc_ce1_ahb_clk.c,     "qseecom"),
	CLK_LOOKUP("bus_clk",      gcc_ce1_axi_clk.c,     "qseecom"),
	CLK_LOOKUP("core_clk_src", ce1_clk_src.c,         "qseecom"),

	CLK_LOOKUP("ce_drv_core_clk",     gcc_ce2_clk.c,         "qseecom"),
	CLK_LOOKUP("ce_drv_iface_clk",    gcc_ce2_ahb_clk.c,     "qseecom"),
	CLK_LOOKUP("ce_drv_bus_clk",      gcc_ce2_axi_clk.c,     "qseecom"),
	CLK_LOOKUP("ce_drv_core_clk_src", ce2_clk_src.c,         "qseecom"),

};

static struct pll_config_regs mmpll10_regs = {
	.l_reg = (void __iomem *)MMPLL10_PLL_L_VAL,
	.m_reg = (void __iomem *)MMPLL10_PLL_M_VAL,
	.n_reg = (void __iomem *)MMPLL10_PLL_N_VAL,
	.config_reg = (void __iomem *)MMPLL10_PLL_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL10_PLL_MODE,
	.base = &virt_bases[GCC_BASE],
};

/* PLL4 at 345.6 MHz, main output enabled.*/
static struct pll_config mmpll10_config = {
	.m = 0,
	.n = 1,
	.vco_val = 0,
	.vco_mask = BM(29, 28),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = BIT(8),
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs gpll4_regs __initdata = {
	.l_reg = (void __iomem *)GPLL4_L,
	.m_reg = (void __iomem *)GPLL4_M,
	.n_reg = (void __iomem *)GPLL4_N,
	.config_reg = (void __iomem *)GPLL4_USER_CTL,
	.mode_reg = (void __iomem *)GPLL4_MODE,
	.base = &virt_bases[GCC_BASE],
};

/* PLL4 at 288 MHz, main output enabled. LJ mode. */
static struct pll_config gpll4_config __initdata = {
	.l = 0x1e,
	.m = 0x0,
	.n = 0x1,
	.vco_val = 0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = BIT(8),
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static void __init reg_init(void)
{
	u32 regval;

	configure_sr_hpm_lp_pll(&gpll4_config, &gpll4_regs, 1);

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	regval = readl_relaxed(
			GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
	writel_relaxed(regval | BIT(26) | BIT(25),
			GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
}

static void __init fsm9900_clock_post_init(void)
{
	/* Set rates for single-rate clocks. */
	clk_set_rate(&usb_hs_system_clk_src.c,
			usb_hs_system_clk_src.freq_tbl[0].freq_hz);
}

#define GCC_CC_PHYS		0xFC400000
#define GCC_CC_SIZE		SZ_16K

#define APCS_GCC_CC_PHYS	0xF9011000
#define APCS_GCC_CC_SIZE	SZ_4K

static void __init fsm9900_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-fsm9900: Unable to ioremap GCC memory!");

	virt_bases[APCS_BASE] = ioremap(APCS_GCC_CC_PHYS, APCS_GCC_CC_SIZE);
	if (!virt_bases[APCS_BASE])
		panic("clock-fsm9900: Unable to ioremap APCS_GCC_CC memory!");

	clk_ops_local_pll.enable = sr_hpm_lp_pll_clk_enable;

	/* This chip does not allow vdd_dig to be modified after bootup */
	regulator_use_dummy_regulator();
	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");

	reg_init();
}

struct clock_init_data fsm9900_clock_init_data __initdata = {
	.table = fsm_clocks_9900,
	.size = ARRAY_SIZE(fsm_clocks_9900),
	.pre_init = fsm9900_clock_pre_init,
	.post_init = fsm9900_clock_post_init,
};


/* These tables are for use in sim and rumi targets */

static struct clk_lookup fsm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP2_UART_CLK, "f9960000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP2_UART_CLK, "f9960000.serial", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("core_clk",   BLSP2_I2C_CLK,  "f9966000.i2c",    OFF),
	CLK_DUMMY("iface_clk",  BLSP2_I2C_CLK,  "f9966000.i2c",    OFF),
	CLK_DUMMY("core_clk",   BLSP1_I2C_CLK,  "f9924000.i2c",    OFF),
	CLK_DUMMY("iface_clk",  BLSP1_I2C_CLK,  "f9924000.i2c",    OFF),
	CLK_DUMMY("core_clk",   NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("iface_clk",  NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("phy_clk",    NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("xo",         NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("core_clk",   NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("iface_clk",  NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("sleep_clk",  NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("xo",         NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("core_clk",   NULL,           "f9824900.sdhci_msm", OFF),
	CLK_DUMMY("iface_clk",  NULL,           "f9824900.sdhci_msm", OFF),
	CLK_DUMMY("core_clk",   NULL,           "f98a4900.sdhci_msm", OFF),
	CLK_DUMMY("iface_clk",  NULL,           "f98a4900.sdhci_msm", OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	SDC2_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("iface_clk",	SDC2_P_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fd440000.qcom,qcrypto", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fd440000.qcom,qcrypto", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fd440000.qcom,qcrypto", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fd440000.qcom,qcrypto", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe040000.qcom,qcrypto", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe040000.qcom,qcrypto", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe040000.qcom,qcrypto", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe040000.qcom,qcrypto", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe000000.qcom,qcrypto", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe000000.qcom,qcrypto", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe000000.qcom,qcrypto", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe000000.qcom,qcrypto", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe140000.qcom,qcota", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe140000.qcom,qcota", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe140000.qcom,qcota", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe140000.qcom,qcota", OFF),

	CLK_DUMMY("core_clk",     NULL,         "fe0c0000.qcom,qcota", OFF),
	CLK_DUMMY("iface_clk",    NULL,         "fe0c0000.qcom,qcota", OFF),
	CLK_DUMMY("bus_clk",      NULL,         "fe0c0000.qcom,qcota", OFF),
	CLK_DUMMY("core_clk_src", NULL,         "fe0c0000.qcom,qcota", OFF),

	CLK_DUMMY("dma_bam_pclk", NULL,         "msm_sps",             OFF),
	CLK_DUMMY("dfab_clk",     NULL,         "msm_sps",             OFF),

	CLK_DUMMY("iface_clk",     NULL,         "f9bff000.qcom,msm-rng", OFF),
};

struct clock_init_data fsm9900_dummy_clock_init_data __initdata = {
	.table = fsm_clocks_dummy,
	.size = ARRAY_SIZE(fsm_clocks_dummy),
};

void mpll10_326_clk_init(void)
{
	mmpll10_config.l = 0x11;
	configure_sr_hpm_lp_pll(&mmpll10_config, &mmpll10_regs, 1);
}

void mpll10_345_clk_init(void)
{
	mmpll10_config.l = 0x12;
	configure_sr_hpm_lp_pll(&mmpll10_config, &mmpll10_regs, 1);
}
