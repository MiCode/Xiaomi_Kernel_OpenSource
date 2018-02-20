/*
 * Copyright (c) 2014-2016, 2018, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CLOCKS_8952_HWIO_H
#define __MSM_CLOCKS_8952_HWIO_H

#define GPLL0_MODE			0x21000
#define GPLL0_STATUS			0x2101C
#define GPLL6_STATUS			0x3701C
#define GPLL3_MODE			0x22000
#define GPLL4_MODE			0x24000
#define GPLL4_STATUS			0x24024
#define GX_DOMAIN_MISC			0x5B00C
#define SYS_MM_NOC_AXI_CBCR		0x3D008
#define BIMC_GFX_CBCR			0x59034
#define MSS_CFG_AHB_CBCR		0x49000
#define	MSS_Q6_BIMC_AXI_CBCR		0x49004
#define USB_HS_BCR			0x41000
#define USB_HS_SYSTEM_CBCR		0x41004
#define USB_HS_AHB_CBCR			0x41008
#define USB_HS_PHY_CFG_AHB_CBCR		0x41030
#define USB_HS_SYSTEM_CMD_RCGR		0x41010
#define USB2A_PHY_SLEEP_CBCR		0x4102C
#define USB_FS_SYSTEM_CBCR		0x3F004
#define USB_FS_AHB_CBCR			0x3F008
#define USB_FS_IC_CBCR			0x3F030
#define USB_FS_SYSTEM_CMD_RCGR		0x3F010
#define USB_FS_IC_CMD_RCGR		0x3F034
#define USB2_HS_PHY_ONLY_BCR		0x41034
#define QUSB2_PHY_BCR			0x4103C
#define SDCC1_APPS_CMD_RCGR		0x42004
#define SDCC1_APPS_CBCR			0x42018
#define SDCC1_AHB_CBCR			0x4201C
#define SDCC1_ICE_CORE_CMD_RCGR		0x5D000
#define SDCC1_ICE_CORE_CBCR		0x5D014
#define SDCC2_APPS_CMD_RCGR		0x43004
#define SDCC2_APPS_CBCR			0x43018
#define SDCC2_AHB_CBCR			0x4301C
#define BLSP1_AHB_CBCR			0x01008
#define BLSP1_QUP1_SPI_APPS_CBCR	0x02004
#define BLSP1_QUP1_I2C_APPS_CBCR	0x02008
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR	0x0200C
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR	0x03000
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR	0x04000
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR	0x05000
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR	0x02024
#define BLSP1_UART1_APPS_CBCR		0x0203C
#define BLSP1_UART1_APPS_CMD_RCGR	0x02044
#define BLSP1_QUP2_SPI_APPS_CBCR	0x0300C
#define BLSP1_QUP2_I2C_APPS_CBCR	0x03010
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR	0x03014
#define BLSP1_UART2_APPS_CBCR		0x0302C
#define BLSP1_UART2_APPS_CMD_RCGR	0x03034
#define BLSP1_QUP3_SPI_APPS_CBCR	0x0401C
#define BLSP1_QUP3_I2C_APPS_CBCR	0x04020
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR	0x04024
#define BLSP1_QUP4_SPI_APPS_CBCR	0x0501C
#define BLSP1_QUP4_I2C_APPS_CBCR	0x05020
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR	0x05024
#define BLSP2_AHB_CBCR			0x0B008
#define BLSP2_QUP1_SPI_APPS_CBCR	0x0C004
#define BLSP2_QUP1_I2C_APPS_CBCR	0x0C008
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR	0x0C00C
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR	0x0D000
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR	0x0F000
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR	0x18000
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR	0x0C024
#define BLSP2_UART1_APPS_CBCR		0x0C03C
#define BLSP2_UART1_APPS_CMD_RCGR	0x0C044
#define BLSP2_QUP2_SPI_APPS_CBCR	0x0D00C
#define BLSP2_QUP2_I2C_APPS_CBCR	0x0D010
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR	0x0D014
#define BLSP2_UART2_APPS_CBCR		0x0D02C
#define BLSP2_UART2_APPS_CMD_RCGR	0x0D034
#define BLSP2_QUP3_SPI_APPS_CBCR	0x0F01C
#define BLSP2_QUP3_I2C_APPS_CBCR	0x0F020
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR	0x0F024
#define BLSP2_QUP4_SPI_APPS_CBCR	0x1801C
#define BLSP2_QUP4_I2C_APPS_CBCR	0x18020
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR	0x18024
#define PDM_AHB_CBCR			0x44004
#define PDM2_CBCR			0x4400C
#define PDM2_CMD_RCGR			0x44010
#define PRNG_AHB_CBCR			0x13004
#define BOOT_ROM_AHB_CBCR		0x1300C
#define CRYPTO_CMD_RCGR			0x16004
#define CRYPTO_CBCR			0x1601C
#define CRYPTO_AXI_CBCR			0x16020
#define CRYPTO_AHB_CBCR			0x16024
#define GCC_XO_DIV4_CBCR		0x30034
#define APSS_AHB_CMD_RCGR		0x46000
#define GCC_PLLTEST_PAD_CFG		0x7400C
#define GFX_TBU_CBCR			0x12010
#define VENUS_TBU_CBCR			0x12014
#define APSS_TCU_CBCR			0x12018
#define MDP_TBU_CBCR			0x1201C
#define GFX_TCU_CBCR			0x12020
#define JPEG_TBU_CBCR			0x12034
#define SMMU_CFG_CBCR			0x12038
#define QDSS_DAP_CBCR			0x29084
#define VFE_TBU_CBCR			0x1203C
#define VFE1_TBU_CBCR			0x12090
#define CPP_TBU_CBCR			0x12040
#define APCS_GPLL_ENA_VOTE		0x45000
#define APCS_CLOCK_BRANCH_ENA_VOTE	0x45004
#define APCS_SMMU_CLOCK_BRANCH_ENA_VOTE	0x4500C
#define GCC_DEBUG_CLK_CTL		0x74000
#define CLOCK_FRQ_MEASURE_CTL		0x74004
#define CLOCK_FRQ_MEASURE_STATUS	0x74008
#define GP1_CBCR			0x08000
#define GP1_CMD_RCGR			0x08004
#define GP1_CFG_RCGR			0x08008
#define GP2_CBCR			0x09000
#define GP2_CMD_RCGR			0x09004
#define GP3_CBCR			0x0A000
#define GP3_CMD_RCGR			0x0A004
#define VCODEC0_CMD_RCGR		0x4C000
#define VENUS0_VCODEC0_CBCR		0x4C01C
#define VENUS0_CORE0_VCODEC0_CBCR	0x4C02C
#define VENUS0_CORE1_VCODEC0_CBCR	0x4C034
#define VENUS0_AHB_CBCR			0x4C020
#define VENUS0_AXI_CBCR			0x4C024
#define PCLK0_CMD_RCGR			0x4D000
#define MDP_CMD_RCGR			0x4D014
#define VSYNC_CMD_RCGR			0x4D02C
#define BYTE0_CMD_RCGR			0x4D044
#define ESC0_CMD_RCGR			0x4D05C
#define MDSS_AHB_CBCR			0x4D07C
#define MDSS_AXI_CBCR			0x4D080
#define MDSS_PCLK0_CBCR			0x4D084
#define MDSS_MDP_CBCR			0x4D088
#define MDSS_VSYNC_CBCR			0x4D090
#define MDSS_BYTE0_CBCR			0x4D094
#define MDSS_ESC0_CBCR			0x4D098
#define CSI0PHYTIMER_CMD_RCGR		0x4E000
#define CAMSS_CSI0PHYTIMER_CBCR		0x4E01C
#define CSI0_CMD_RCGR			0x4E020
#define CAMSS_CSI0_CBCR			0x4E03C
#define CAMSS_CSI0_AHB_CBCR		0x4E040
#define CAMSS_CSI0PHY_CBCR		0x4E048
#define CAMSS_CSI0RDI_CBCR		0x4E050
#define CAMSS_CSI0PIX_CBCR		0x4E058
#define CSI1PHYTIMER_CMD_RCGR		0x4F000
#define CSI1_CMD_RCGR			0x4F020
#define CAMSS_CSI1_CBCR			0x4F03C
#define CAMSS_CSI1PHYTIMER_CBCR		0x4F01C
#define CAMSS_CSI1_AHB_CBCR		0x4F040
#define CAMSS_CSI1PHY_CBCR		0x4F048
#define CAMSS_CSI1RDI_CBCR		0x4F050
#define CAMSS_CSI1PIX_CBCR		0x4F058
#define CSI2_CMD_RCGR			0x3C020
#define CAMSS_CSI2_CBCR			0x3C03C
#define CAMSS_CSI2_AHB_CBCR		0x3C040
#define CAMSS_CSI2PHY_CBCR		0x3C048
#define CAMSS_CSI2RDI_CBCR		0x3C050
#define CAMSS_CSI2PIX_CBCR		0x3C058
#define CAMSS_ISPIF_AHB_CBCR		0x50004
#define CCI_CMD_RCGR			0x51000
#define CAMSS_CCI_CBCR			0x51018
#define CAMSS_CCI_AHB_CBCR		0x5101C
#define MCLK0_CMD_RCGR			0x52000
#define CAMSS_MCLK0_CBCR		0x52018
#define MCLK1_CMD_RCGR			0x53000
#define CAMSS_MCLK1_CBCR		0x53018
#define MCLK2_CMD_RCGR			0x5C000
#define CAMSS_MCLK2_CBCR		0x5C018
#define MM_GP0_CMD_RCGR			0x54000
#define CAMSS_GP0_CBCR			0x54018
#define MM_GP1_CMD_RCGR			0x55000
#define CAMSS_GP1_CBCR			0x55018
#define CAMSS_TOP_AHB_CBCR		0x5A014
#define CAMSS_AHB_CBCR			0x56004
#define CAMSS_MICRO_AHB_CBCR		0x5600C
#define CAMSS_MICRO_BCR			0x56008
#define JPEG0_CMD_RCGR			0x57000
#define CAMSS_JPEG0_CBCR		0x57020
#define CAMSS_JPEG_AHB_CBCR		0x57024
#define CAMSS_JPEG_AXI_CBCR		0x57028
#define VFE0_CMD_RCGR			0x58000
#define CPP_CMD_RCGR			0x58018
#define CAMSS_VFE0_CBCR			0x58038
#define CAMSS_CPP_CBCR			0x5803C
#define CAMSS_CPP_AHB_CBCR		0x58040
#define CAMSS_VFE_AHB_CBCR		0x58044
#define CAMSS_VFE_AXI_CBCR		0x58048
#define CAMSS_CSI_VFE0_CBCR		0x58050
#define VFE1_CMD_RCGR			0x58054
#define CAMSS_VFE1_CBCR			0x5805C
#define CAMSS_VFE1_AHB_CBCR		0x58060
#define CAMSS_CPP_AXI_CBCR		0x58064
#define CAMSS_VFE1_AXI_CBCR		0x58068
#define CAMSS_CSI_VFE1_CBCR		0x58074
#define GFX3D_CMD_RCGR			0x59000
#define OXILI_GFX3D_CBCR		0x59020
#define OXILI_GMEM_CBCR			0x59024
#define OXILI_AHB_CBCR			0x59028
#define OXILI_TIMER_CBCR		0x59040
#define OXILI_AON_CBCR			0x5904C
#define CAMSS_TOP_AHB_CMD_RCGR		0x5A000
#define BIMC_GPU_CBCR			0x59030
#define GTCU_AHB_CBCR			0x12044
#define IPA_TBU_CBCR			0x120A0
#define SYSTEM_MM_NOC_CMD_RCGR		0x3D000
#define USB_FS_BCR			0x3F000

#define APCS_CLOCK_SLEEP_ENA_VOTE	0x45008
#define BYTE1_CMD_RCGR			0x4D0B0
#define ESC1_CMD_RCGR			0x4D0A8
#define PCLK1_CMD_RCGR			0x4D0B8
#define MDSS_BYTE1_CBCR			0x4D0A0
#define MDSS_ESC1_CBCR			0x4D09C
#define MDSS_PCLK1_CBCR			0x4D0A4
#define DCC_CBCR			0x77004

#define RPM_MISC_CLK_TYPE		0x306b6c63
#define RPM_BUS_CLK_TYPE		0x316b6c63
#define RPM_MEM_CLK_TYPE		0x326b6c63
#define RPM_IPA_CLK_TYPE		0x617069
#define RPM_SMD_KEY_ENABLE		0x62616E45

#define CXO_CLK_SRC_ID			0x0
#define QDSS_CLK_ID			0x1

#define PNOC_CLK_ID			0x0
#define SNOC_CLK_ID			0x1
#define SYSMMNOC_CLK_ID			0x2
#define BIMC_CLK_ID			0x0
#define BIMC_GPU_CLK_ID			0x2
#define IPA_CLK_ID			0x0

#define BUS_SCALING		0x2

/* XO clock */
#define BB_CLK1_ID		0x1
#define BB_CLK2_ID		0x2
#define RF_CLK2_ID		0x5
#define LN_BB_CLK_ID		0x8
#define DIV_CLK1_ID		0xb
#define DIV_CLK2_ID		0xc

#define APCS_CCI_PLL_MODE		0x00000
#define APCS_CCI_PLL_L_VAL		0x00004
#define APCS_CCI_PLL_M_VAL		0x00008
#define APCS_CCI_PLL_N_VAL		0x0000C
#define APCS_CCI_PLL_USER_CTL		0x00010
#define APCS_CCI_PLL_CONFIG_CTL		0x00014
#define APCS_CCI_PLL_STATUS		0x0001C

#define APCS_C0_PLL_MODE		0x00000
#define APCS_C0_PLL_L_VAL		0x00004
#define APCS_C0_PLL_M_VAL		0x00008
#define APCS_C0_PLL_N_VAL		0x0000C
#define APCS_C0_PLL_USER_CTL		0x00010
#define APCS_C0_PLL_CONFIG_CTL		0x00014
#define APCS_C0_PLL_STATUS		0x0001C

#define APCS_C1_PLL_MODE		0x00000
#define APCS_C1_PLL_L_VAL		0x00004
#define APCS_C1_PLL_M_VAL		0x00008
#define APCS_C1_PLL_N_VAL		0x0000C
#define APCS_C1_PLL_USER_CTL		0x00010
#define APCS_C1_PLL_CONFIG_CTL		0x00014
#define APCS_C1_PLL_STATUS		0x0001C


#define CLKFLAG_WAKEUP_CYCLES		0x0
#define CLKFLAG_SLEEP_CYCLES		0x0

/* Mux source select values */
#define xo_source_val			0
#define xo_a_source_val			0
#define gpll0_source_val		1
#define gpll3_source_val		2
#define gpll0_out_main_source_val	1   /* sdcc1_ice_core */
/* cci_clk_src and usb_fs_system_clk_src */
#define gpll0_out_aux_source_val	2
#define gpll4_source_val		2   /* sdcc1_apss_clk_src */
#define gpll4_out_source_val		3   /* sdcc1_apss_clk_src */
#define gpll6_source_val		2   /* mclk0_2_clk_src */
#define gpll6_aux_source_val		3   /* gfx3d_clk_src */
#define gpll6_out_main_source_val	1   /* usb_fs_ic_clk_src */
#define dsi0_phypll_source_val		1
#define dsi0_0phypll_source_val		1   /* byte0_clk & pclk0_clk */
#define dsi0_1phypll_source_val         3   /* byte1_clk & pclk1_clk */
#define dsi1_0phypll_source_val         3   /* byte0_clk & pclk0_clk */
#define dsi1_1phypll_source_val         1   /* byte1_clk & pclk1_clk */


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

#define F_SLEW(f, s_f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_freq = (s_f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_APCS_PLL(f, l, m, n, pre_div, post_div, vco) \
	{ \
		.freq_hz = (f), \
		.l_val = (l), \
		.m_val = (m), \
		.n_val = (n), \
		.pre_div_val = BVAL(12, 12, (pre_div)), \
		.post_div_val = BVAL(9, 8, (post_div)), \
		.vco_val = BVAL(29, 28, (vco)), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

# define OVERRIDE_FMAX1(clkname, l1, f1) \
	clkname##_clk_src.c.fmax[VDD_DIG_##l1] = (f1)

# define OVERRIDE_FMAX2(clkname, l1, f1, l2, f2) \
	clkname##_clk_src.c.fmax[VDD_DIG_##l1] = (f1);  \
	clkname##_clk_src.c.fmax[VDD_DIG_##l2] = (f2)

#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
		[VDD_DIG_##l3] = (f3),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

# define OVERRIDE_FMAX3(clkname, l1, f1, l2, f2, l3, f3) \
	clkname##_clk_src.c.fmax[VDD_DIG_##l1] = (f1);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l2] = (f2);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l3] = (f3)


# define OVERRIDE_FMAX4(clkname, l1, f1, l2, f2, l3, f3, l4, f4) \
	clkname##_clk_src.c.fmax[VDD_DIG_##l1] = (f1);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l2] = (f2);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l3] = (f3);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l4] = (f4)

#define VDD_DIG_FMAX_MAP5(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),\
		[VDD_DIG_##l2] = (f2),\
		[VDD_DIG_##l3] = (f3),\
		[VDD_DIG_##l4] = (f4),\
		[VDD_DIG_##l5] = (f5),\
	},\
	.num_fmax = VDD_DIG_NUM

#define OVERRIDE_FMAX5(clkname, l1, f1, l2, f2, l3, f3, l4, f4, l5, f5) \
	clkname##_clk_src.c.fmax[VDD_DIG_##l1] = (f1);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l2] = (f2);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l3] = (f3);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l4] = (f4);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l5] = (f5)

#define OVERRIDE_FMAX6(clkname, \
		l1, f1, l2, f2, l3, f3, l4, f4, l5, f5, l6, f6) \
	clkname##_clk_src.c.fmax[VDD_DIG_##l1] = (f1);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l2] = (f2);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l3] = (f3);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l4] = (f4);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l5] = (f5);\
	clkname##_clk_src.c.fmax[VDD_DIG_##l6] = (f6)

#define OVERRIDE_FTABLE(clkname, ftable, name) \
	clkname##_clk_src.freq_tbl = ftable##_##name

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOWER,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_NOM_PLUS,
	VDD_DIG_HIGH,
	VDD_DIG_SUPER_TUR,
	VDD_DIG_NUM
};

enum vdd_dig_levels_8917 {
	VDD_DIG_NONE_8917,
	VDD_DIG_LOWER_8917,
	VDD_DIG_LOW_8917,
	VDD_DIG_NOMINAL_8917,
	VDD_DIG_NOM_PLUS_8917,
	VDD_DIG_HIGH_8917,
	VDD_DIG_NUM_8917
};

enum vdd_hf_pll_levels_8917 {
	VDD_HF_PLL_OFF_8917,
	VDD_HF_PLL_SVS_8917,
	VDD_HF_PLL_NOM_8917,
	VDD_HF_PLL_TUR_8917,
	VDD_HF_PLL_NUM_8917,
};

static int vdd_corner[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_DIG_NONE */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_DIG_SVS */
	RPM_REGULATOR_LEVEL_SVS_PLUS,		/* VDD_DIG_SVS_PLUS */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_DIG_NOM */
	RPM_REGULATOR_LEVEL_NOM_PLUS,		/* VDD_DIG_NOM_PLUS */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_DIG_TURBO */
	RPM_REGULATOR_LEVEL_BINNING,		/* VDD_DIG_SUPER_TUR */
};
#endif
