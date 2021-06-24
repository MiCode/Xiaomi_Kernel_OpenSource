// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6879-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs imp0_cg_regs = {
	.set_ofs = 0x282E08,
	.clr_ofs = 0x282E04,
	.sta_ofs = 0x282E00,
};

static const struct mtk_gate_regs imp1_cg_regs = {
	.set_ofs = 0xCB3E08,
	.clr_ofs = 0xCB3E04,
	.sta_ofs = 0xCB3E00,
};

static const struct mtk_gate_regs imp2_cg_regs = {
	.set_ofs = 0xE03E08,
	.clr_ofs = 0xE03E04,
	.sta_ofs = 0xE03E00,
};

static const struct mtk_gate_regs imp3_cg_regs = {
	.set_ofs = 0xED4E08,
	.clr_ofs = 0xED4E04,
	.sta_ofs = 0xED4E00,
};

#define GATE_IMP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMP3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imp_clks[] = {
	/* IMP0 */
	GATE_IMP0(CLK_IMP_AP_CLOCK_I2C10, "imp_ap_clock_i2c10",
			"i2c_ck"/* parent */, 0),
	GATE_IMP0(CLK_IMP_AP_CLOCK_I2C11, "imp_ap_clock_i2c11",
			"i2c_ck"/* parent */, 1),
	/* IMP1 */
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C2, "imp_ap_clock_i2c2",
			"i2c_ck"/* parent */, 0),
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C4, "imp_ap_clock_i2c4",
			"i2c_ck"/* parent */, 1),
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C9, "imp_ap_clock_i2c9",
			"i2c_ck"/* parent */, 2),
	/* IMP2 */
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C0, "imp_ap_clock_i2c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C1, "imp_ap_clock_i2c1",
			"i2c_ck"/* parent */, 1),
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C6, "imp_ap_clock_i2c6",
			"i2c_ck"/* parent */, 2),
	/* IMP3 */
	GATE_IMP3(CLK_IMP_AP_CLOCK_I2C3, "imp_ap_clock_i2c3",
			"i2c_ck"/* parent */, 0),
	GATE_IMP3(CLK_IMP_AP_CLOCK_I2C5, "imp_ap_clock_i2c5",
			"i2c_ck"/* parent */, 1),
	GATE_IMP3(CLK_IMP_AP_CLOCK_I2C7, "imp_ap_clock_i2c7",
			"i2c_ck"/* parent */, 2),
	GATE_IMP3(CLK_IMP_AP_CLOCK_I2C8, "imp_ap_clock_i2c8",
			"i2c_ck"/* parent */, 3),
};

static const struct mtk_clk_desc imp_mcd = {
	.clks = imp_clks,
	.num_clks = ARRAY_SIZE(imp_clks),
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x404,
	.clr_ofs = 0x404,
	.sta_ofs = 0x404,
};

static const struct mtk_gate_regs perao3_cg_regs = {
	.set_ofs = 0x408,
	.clr_ofs = 0x408,
	.sta_ofs = 0x408,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_PERAO3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAOP_UART0, "peraop_uart0",
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAOP_UART2, "peraop_uart2",
			"uart_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAOP_PWM_HCLK, "peraop_pwm_hclk",
			"axip_ck"/* parent */, 3),
	GATE_PERAO0(CLK_PERAOP_PWM_BCLK, "peraop_pwm_bclk",
			"pwm_ck"/* parent */, 4),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK1, "peraop_pwm_fbclk1",
			"pwm_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK2, "peraop_pwm_fbclk2",
			"pwm_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK3, "peraop_pwm_fbclk3",
			"pwm_ck"/* parent */, 7),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK4, "peraop_pwm_fbclk4",
			"pwm_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAOP_BTIF_BCLK, "peraop_btif_bclk",
			"axip_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAOP_DISP_PWM0, "peraop_disp_pwm0",
			"disp_pwm_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAOP_SPI0_BCLK, "peraop_spi0_bclk",
			"spi_ck"/* parent */, 11),
	GATE_PERAO0(CLK_PERAOP_SPI1_BCLK, "peraop_spi1_bclk",
			"spi_ck"/* parent */, 12),
	GATE_PERAO0(CLK_PERAOP_SPI2_BCLK, "peraop_spi2_bclk",
			"spi_ck"/* parent */, 13),
	GATE_PERAO0(CLK_PERAOP_SPI3_BCLK, "peraop_spi3_bclk",
			"spi_ck"/* parent */, 14),
	GATE_PERAO0(CLK_PERAOP_SPI4_BCLK, "peraop_spi4_bclk",
			"spi_ck"/* parent */, 15),
	GATE_PERAO0(CLK_PERAOP_SPI5_BCLK, "peraop_spi5_bclk",
			"spi_ck"/* parent */, 16),
	GATE_PERAO0(CLK_PERAOP_SPI6_BCLK, "peraop_spi6_bclk",
			"spi_ck"/* parent */, 17),
	GATE_PERAO0(CLK_PERAOP_SPI7_BCLK, "peraop_spi7_bclk",
			"spi_ck"/* parent */, 18),
	GATE_PERAO0(CLK_PERAOP_SPI0_HCLK, "peraop_spi0_hclk",
			"spi_ck"/* parent */, 19),
	GATE_PERAO0(CLK_PERAOP_SPI1_HCLK, "peraop_spi1_hclk",
			"axip_ck"/* parent */, 20),
	GATE_PERAO0(CLK_PERAOP_SPI2_HCLK, "peraop_spi2_hclk",
			"axip_ck"/* parent */, 21),
	GATE_PERAO0(CLK_PERAOP_SPI3_HCLK, "peraop_spi3_hclk",
			"axip_ck"/* parent */, 22),
	GATE_PERAO0(CLK_PERAOP_SPI4_HCLK, "peraop_spi4_hclk",
			"axip_ck"/* parent */, 23),
	GATE_PERAO0(CLK_PERAOP_SPI5_HCLK, "peraop_spi5_hclk",
			"axip_ck"/* parent */, 24),
	GATE_PERAO0(CLK_PERAOP_SPI6_HCLK, "peraop_spi6_hclk",
			"axip_ck"/* parent */, 25),
	GATE_PERAO0(CLK_PERAOP_SPI7_HCLK, "peraop_spi7_hclk",
			"axip_ck"/* parent */, 26),
	GATE_PERAO0(CLK_PERAOP_IIC, "peraop_iic",
			"axip_ck"/* parent */, 27),
	GATE_PERAO0(CLK_PERAOP_APDMA, "peraop_apdma",
			"axip_ck"/* parent */, 28),
	GATE_PERAO0(CLK_PERAOP_USB_PCLK, "peraop_usb_pclk",
			"f26m_ck"/* parent */, 29),
	GATE_PERAO0(CLK_PERAOP_USB_REF, "peraop_usb_ref",
			"f26m_ck"/* parent */, 30),
	GATE_PERAO0(CLK_PERAOP_USB_FRMCNT, "peraop_usb_frmcnt",
			"univpll_192m_ck"/* parent */, 31),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_USB_PHY, "peraop_usb_phy",
			"f26m_ck"/* parent */, 0),
	GATE_PERAO1(CLK_PERAOP_USB_SYS, "peraop_usb_sys",
			"usb_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAOP_USB_XHCI, "peraop_usb_xhci",
			"ssusb_xhci_ck"/* parent */, 2),
	GATE_PERAO1(CLK_PERAOP_USB_DMA_BUS, "peraop_usb_dma_bus",
			"axip_ck"/* parent */, 3),
	GATE_PERAO1(CLK_PERAOP_USB_MCU_BUS, "peraop_usb_mcu_bus",
			"axip_ck"/* parent */, 4),
	GATE_PERAO1(CLK_PERAOP_MSDC1, "peraop_msdc1",
			"msdc30_1_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAOP_MSDC1_HCLK, "peraop_msdc1_hclk",
			"axip_ck"/* parent */, 6),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAOP_UART0_SEN, "peraop_uart0_sen",
			"uart_ck"/* parent */, 0),
	/* PERAO3 */
	GATE_PERAO3(CLK_PERAOP_USB_PHY_SEN, "peraop_usb_phy_sen",
			"f26m_ck"/* parent */, 0),
	GATE_PERAO3(CLK_PERAOP_USB_SYS_SEN, "peraop_usb_sys_sen",
			"usb_ck"/* parent */, 1),
	GATE_PERAO3(CLK_PERAOP_USB_XHCI_SEN, "peraop_usb_xhci_sen",
			"ssusb_xhci_ck"/* parent */, 2),
	GATE_PERAO3(CLK_PERAOP_USB_DMA_B, "peraop_usb_dma_b",
			"axip_ck"/* parent */, 3),
	GATE_PERAO3(CLK_PERAOP_USB_MCU_B, "peraop_usb_mcu_b",
			"axip_ck"/* parent */, 4),
	GATE_PERAO3(CLK_PERAOP_MSDC1_SEN, "peraop_msdc1_sen",
			"msdc30_1_ck"/* parent */, 5),
	GATE_PERAO3(CLK_PERAOP_MSDC1_HCLK_SEN, "peraop_msdc1_h_sen",
			"axip_ck"/* parent */, 6),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = ARRAY_SIZE(perao_clks),
};

static const struct mtk_gate_regs usb_d_cg_regs = {
	.set_ofs = 0xC84,
	.clr_ofs = 0xC84,
	.sta_ofs = 0xC84,
};

#define GATE_USB_D(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_d_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate usb_d_clks[] = {
	GATE_USB_D(CLK_USB_D_DMA_B, "usb_d_dma_b",
			"axi_ck"/* parent */, 2),
};

static const struct mtk_clk_desc usb_d_mcd = {
	.clks = usb_d_clks,
	.num_clks = ARRAY_SIZE(usb_d_clks),
};

static const struct mtk_gate_regs usb_s0_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x30,
	.sta_ofs = 0x30,
};

static const struct mtk_gate_regs usb_s1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x50,
	.sta_ofs = 0x50,
};

static const struct mtk_gate_regs usb_s2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_USB_S0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_s0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_USB_S1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_s1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_USB_S2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_s2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate usb_s_clks[] = {
	/* USB_S0 */
	GATE_USB_S0(CLK_USB_S_USB_U3_P, "usb_s_usb_u3_p",
			"usb_ck"/* parent */, 0),
	/* USB_S1 */
	GATE_USB_S1(CLK_USB_S_USB_U2_P, "usb_s_usb_u2_p",
			"usb_ck"/* parent */, 0),
	/* USB_S2 */
	GATE_USB_S2(CLK_USB_S_USB_IP_DMA_B, "usb_s_usb_ip_dma_b",
			"axi_ck"/* parent */, 0),
};

static const struct mtk_clk_desc usb_s_mcd = {
	.clks = usb_s_clks,
	.num_clks = ARRAY_SIZE(usb_s_clks),
};

static const struct mtk_gate_regs ufsao0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ufsao1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0xC,
};

#define GATE_UFSAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_UFSAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufsao_clks[] = {
	/* UFSAO0 */
	GATE_UFSAO0(CLK_UFSAO_PERI2UFS_0C, "ufsao_peri2ufs_0c",
			"axi_u_ck"/* parent */, 0),
	GATE_UFSAO0(CLK_UFSAO_U_PHY_SAP_0C, "ufsao_u_phy_sap_0c",
			"f26m_ck"/* parent */, 1),
	GATE_UFSAO0(CLK_UFSAO_U_PHY_TOP_AHB_S_0C, "ufsao_u_phy_ahb_s_0c",
			"axi_u_ck"/* parent */, 2),
	GATE_UFSAO0(CLK_UFSAO_U_TX_SYMBOL_0C, "ufsao_u_tx_symbol_0c",
			"f26m_ck"/* parent */, 3),
	GATE_UFSAO0(CLK_UFSAO_U_RX_SYMBOL_0C, "ufsao_u_rx_symbol_0c",
			"f26m_ck"/* parent */, 4),
	GATE_UFSAO0(CLK_UFSAO_U_RX_SYM1_0C, "ufsao_u_rx_sym1_0c",
			"f26m_ck"/* parent */, 5),
	/* UFSAO1 */
	GATE_UFSAO1(CLK_UFSAO_PERI2UFS_0, "ufsao_peri2ufs_0",
			"axi_u_ck"/* parent */, 0),
	GATE_UFSAO1(CLK_UFSAO_U_PHY_SAP_0, "ufsao_u_phy_sap_0",
			"f26m_ck"/* parent */, 1),
	GATE_UFSAO1(CLK_UFSAO_U_PHY_TOP_AHB_S_0, "ufsao_u_phy_ahb_s_0",
			"axi_u_ck"/* parent */, 2),
	GATE_UFSAO1(CLK_UFSAO_U_TX_SYMBOL_0, "ufsao_u_tx_symbol_0",
			"f26m_ck"/* parent */, 3),
	GATE_UFSAO1(CLK_UFSAO_U_RX_SYMBOL_0, "ufsao_u_rx_symbol_0",
			"f26m_ck"/* parent */, 4),
	GATE_UFSAO1(CLK_UFSAO_U_RX_SYM1_0, "ufsao_u_rx_sym1_0",
			"f26m_ck"/* parent */, 5),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = ARRAY_SIZE(ufsao_clks),
};

static const struct mtk_gate_regs ufspdn0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ufspdn1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0xC,
};

#define GATE_UFSPDN0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufspdn0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_UFSPDN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufspdn1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufspdn_clks[] = {
	/* UFSPDN0 */
	GATE_UFSPDN0(CLK_UFSPDN_UFS2PERI_GLAS_AXI_CON_0, "ufspdn_ufs2peri_sen",
			"mem_sub_ck"/* parent */, 0),
	GATE_UFSPDN0(CLK_UFSPDN_UNIPRO_SYSCLK_CON_0, "ufspdn_upro_sck_sen",
			"ufs_ck"/* parent */, 1),
	GATE_UFSPDN0(CLK_UFSPDN_UNIPRO_TICK1US_CON_0, "ufspdn_upro_tick_sen",
			"f_u_tick1us_ck"/* parent */, 2),
	GATE_UFSPDN0(CLK_UFSPDN_U_CK_CON_0, "ufspdn_u_sen",
			"ufs_ck"/* parent */, 3),
	GATE_UFSPDN0(CLK_UFSPDN_AES_UFSFDE_CK_CON_0, "ufspdn_aes_u_sen",
			"aes_ufsfde_ck"/* parent */, 4),
	GATE_UFSPDN0(CLK_UFSPDN_U_TICK1US_CK_CON_0, "ufspdn_u_tick_sen",
			"f_u_tick1us_ck"/* parent */, 5),
	GATE_UFSPDN0(CLK_UFSPDN_RG_66M_UFSHCI_TOP_HCLK_CK_CON_0, "ufspdn_ufshci_sen",
			"axi_u_ck"/* parent */, 6),
	GATE_UFSPDN0(CLK_UFSPDN_UFSCI_TOP_FMEM_SUB_CK_CON_0, "ufspdn_mem_sub_sen",
			"mem_sub_ck"/* parent */, 7),
	/* UFSPDN1 */
	GATE_UFSPDN1(CLK_UFSPDN_UFS2PERI_GLAS_AXI_SET_0, "ufspdn_ufs2peri_ck",
			"mem_sub_ck"/* parent */, 0),
	GATE_UFSPDN1(CLK_UFSPDN_UNIPRO_SYSCLK_SET_0, "ufspdn_upro_sck_ck",
			"ufs_ck"/* parent */, 1),
	GATE_UFSPDN1(CLK_UFSPDN_UNIPRO_TICK1US_SET_0, "ufspdn_upro_tick_ck",
			"f_u_tick1us_ck"/* parent */, 2),
	GATE_UFSPDN1(CLK_UFSPDN_U_CK_SET_0, "ufspdn_u_ck",
			"ufs_ck"/* parent */, 3),
	GATE_UFSPDN1(CLK_UFSPDN_AES_UFSFDE_CK_SET_0, "ufspdn_aes_u_ck",
			"aes_ufsfde_ck"/* parent */, 4),
	GATE_UFSPDN1(CLK_UFSPDN_U_TICK1US_CK_SET_0, "ufspdn_u_tick_ck",
			"f_u_tick1us_ck"/* parent */, 5),
	GATE_UFSPDN1(CLK_UFSPDN_RG_66M_UFSHCI_TOP_HCLK_CK_SET_0, "ufspdn_ufshci_ck",
			"axi_u_ck"/* parent */, 6),
	GATE_UFSPDN1(CLK_UFSPDN_UFSCI_TOP_FMEM_SUB_CK_SET_0, "ufspdn_mem_sub_ck",
			"mem_sub_ck"/* parent */, 7),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = ARRAY_SIZE(ufspdn_clks),
};

static const struct of_device_id of_match_clk_mt6879_peri[] = {
	{
		.compatible = "mediatek,mt6879-imp_iic_wrap",
		.data = &imp_mcd,
	}, {
		.compatible = "mediatek,mt6879-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6879-ssusb_device",
		.data = &usb_d_mcd,
	}, {
		.compatible = "mediatek,mt6879-ssusb_sifslv_ippc",
		.data = &usb_s_mcd,
	}, {
		.compatible = "mediatek,mt6879-ufs_ao_config",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6879-ufs_pdn_cfg",
		.data = &ufspdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6879_peri_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6879_peri_drv = {
	.probe = clk_mt6879_peri_grp_probe,
	.driver = {
		.name = "clk-mt6879-peri",
		.of_match_table = of_match_clk_mt6879_peri,
	},
};

static int __init clk_mt6879_peri_init(void)
{
	return platform_driver_register(&clk_mt6879_peri_drv);
}

static void __exit clk_mt6879_peri_exit(void)
{
	platform_driver_unregister(&clk_mt6879_peri_drv);
}

arch_initcall(clk_mt6879_peri_init);
module_exit(clk_mt6879_peri_exit);
MODULE_LICENSE("GPL");
