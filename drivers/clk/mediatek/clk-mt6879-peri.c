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

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(CLK_IMPC_AP_CLOCK_I2C10, "impc_ap_clock_i2c10",
			"i2c_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_AP_CLOCK_I2C11, "impc_ap_clock_i2c11",
			"i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impe_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impe_clks[] = {
	GATE_IMPE(CLK_IMPE_AP_CLOCK_I2C2, "impe_ap_clock_i2c2",
			"i2c_ck"/* parent */, 0),
	GATE_IMPE(CLK_IMPE_AP_CLOCK_I2C4, "impe_ap_clock_i2c4",
			"i2c_ck"/* parent */, 1),
	GATE_IMPE(CLK_IMPE_AP_CLOCK_I2C9, "impe_ap_clock_i2c9",
			"i2c_ck"/* parent */, 2),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
};

static const struct mtk_gate_regs impen_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPEN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impen_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impen_clks[] = {
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C3, "impen_ap_clock_i2c3",
			"i2c_ck"/* parent */, 0),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C5, "impen_ap_clock_i2c5",
			"i2c_ck"/* parent */, 1),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C7, "impen_ap_clock_i2c7",
			"i2c_ck"/* parent */, 2),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C8, "impen_ap_clock_i2c8",
			"i2c_ck"/* parent */, 3),
};

static const struct mtk_clk_desc impen_mcd = {
	.clks = impen_clks,
	.num_clks = CLK_IMPEN_NR_CLK,
};

static const struct mtk_gate_regs impw_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPW(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impw_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impw_clks[] = {
	GATE_IMPW(CLK_IMPW_AP_CLOCK_I2C0, "impw_ap_clock_i2c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMPW(CLK_IMPW_AP_CLOCK_I2C1, "impw_ap_clock_i2c1",
			"i2c_ck"/* parent */, 1),
	GATE_IMPW(CLK_IMPW_AP_CLOCK_I2C6, "impw_ap_clock_i2c6",
			"i2c_ck"/* parent */, 2),
};

static const struct mtk_clk_desc impw_mcd = {
	.clks = impw_clks,
	.num_clks = CLK_IMPW_NR_CLK,
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

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAOP_UART0, "peraop_uart0",
			"f26m_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"f26m_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAOP_UART2, "peraop_uart2",
			"f26m_ck"/* parent */, 2),
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
	GATE_PERAO0(CLK_PERAOP_APDMA, "peraop_apdma",
			"axip_ck"/* parent */, 28),
	GATE_PERAO0(CLK_PERAOP_USB_FRMCNT, "peraop_usb_frmcnt",
			"univpll_192m_ck"/* parent */, 31),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_USB_SYS, "peraop_usb_sys",
			"usb_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAOP_USB_XHCI, "peraop_usb_xhci",
			"ssusb_xhci_ck"/* parent */, 2),
	GATE_PERAO1(CLK_PERAOP_MSDC1, "peraop_msdc1",
			"msdc30_1_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAOP_MSDC1_HCLK, "peraop_msdc1_hclk",
			"axip_ck"/* parent */, 6),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs ufsao_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0xC,
};

#define GATE_UFSAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufsao_clks[] = {
	GATE_UFSAO(CLK_UFSAO_U_PHY_SAP_0, "ufsao_u_phy_sap_0",
			"f26m_ck"/* parent */, 1),
	GATE_UFSAO(CLK_UFSAO_U_TX_SYMBOL_0, "ufsao_u_tx_symbol_0",
			"f26m_ck"/* parent */, 3),
	GATE_UFSAO(CLK_UFSAO_U_RX_SYMBOL_0, "ufsao_u_rx_symbol_0",
			"f26m_ck"/* parent */, 4),
	GATE_UFSAO(CLK_UFSAO_U_RX_SYM1_0, "ufsao_u_rx_sym1_0",
			"f26m_ck"/* parent */, 5),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct mtk_gate_regs ufspdn_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0xC,
};

#define GATE_UFSPDN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufspdn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufspdn_clks[] = {
	GATE_UFSPDN(CLK_UFSPDN_UNIPRO_SYSCLK_SET_0, "ufspdn_upro_sck_ck",
			"ufs_ck"/* parent */, 1),
	GATE_UFSPDN(CLK_UFSPDN_UNIPRO_TICK1US_SET_0, "ufspdn_upro_tick_ck",
			"f_u_tick1us_ck"/* parent */, 2),
	GATE_UFSPDN(CLK_UFSPDN_U_CK_SET_0, "ufspdn_u_ck",
			"ufs_ck"/* parent */, 3),
	GATE_UFSPDN(CLK_UFSPDN_AES_UFSFDE_CK_SET_0, "ufspdn_aes_u_ck",
			"aes_ufsfde_ck"/* parent */, 4),
	GATE_UFSPDN(CLK_UFSPDN_U_TICK1US_CK_SET_0, "ufspdn_u_tick_ck",
			"f_u_tick1us_ck"/* parent */, 5),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = CLK_UFSPDN_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6879_peri[] = {
	{
		.compatible = "mediatek,mt6879-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6879-imp_iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt6879-imp_iic_wrap_en",
		.data = &impen_mcd,
	}, {
		.compatible = "mediatek,mt6879-imp_iic_wrap_w",
		.data = &impw_mcd,
	}, {
		.compatible = "mediatek,mt6879-pericfg_ao",
		.data = &perao_mcd,
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
