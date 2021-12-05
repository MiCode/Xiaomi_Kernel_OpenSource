// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6895-clk.h>

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
	GATE_IMPC(CLK_IMPC_AP_CLOCK_I2C5, "impc_ap_clock_i2c5",
			"i2c_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_AP_CLOCK_I2C6, "impc_ap_clock_i2c6",
			"i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C1, "imps_ap_clock_i2c1",
			"i2c_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C2, "imps_ap_clock_i2c2",
			"i2c_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C3, "imps_ap_clock_i2c3",
			"i2c_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C4, "imps_ap_clock_i2c4",
			"i2c_ck"/* parent */, 3),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C7, "imps_ap_clock_i2c7",
			"i2c_ck"/* parent */, 4),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C8, "imps_ap_clock_i2c8",
			"i2c_ck"/* parent */, 5),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C9, "imps_ap_clock_i2c9",
			"i2c_ck"/* parent */, 6),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
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
};

static const struct mtk_clk_desc impw_mcd = {
	.clks = impw_clks,
	.num_clks = CLK_IMPW_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x3c,
	.clr_ofs = 0x3c,
	.sta_ofs = 0x3c,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x40,
	.clr_ofs = 0x40,
	.sta_ofs = 0x40,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x44,
	.clr_ofs = 0x44,
	.sta_ofs = 0x44,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAOP_0_UART0, "peraop_0_uart0",
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_0_UART1, "peraop_0_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAOP_0_UART2, "peraop_0_uart2",
			"uart_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAOP_0_PWM_HCLK, "peraop_0_pwm_hclk",
			"peri_axi_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAOP_0_PWM_BCLK, "peraop_0_pwm_bclk",
			"pwm_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAOP_0_PWM_FBCLK1, "peraop_0_pwm_fbclk1",
			"pwm_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAOP_0_PWM_FBCLK2, "peraop_0_pwm_fbclk2",
			"pwm_ck"/* parent */, 11),
	GATE_PERAO0(CLK_PERAOP_0_PWM_FBCLK3, "peraop_0_pwm_fbclk3",
			"pwm_ck"/* parent */, 12),
	GATE_PERAO0(CLK_PERAOP_0_PWM_FBCLK4, "peraop_0_pwm_fbclk4",
			"pwm_ck"/* parent */, 13),
	GATE_PERAO0(CLK_PERAOP_0_BTIF, "peraop_0_btif",
			"peri_axi_ck"/* parent */, 14),
	GATE_PERAO0(CLK_PERAOP_0_DISP, "peraop_0_disp",
			"disp_pwm_ck"/* parent */, 15),
	GATE_PERAO0(CLK_PERAOP_0_DISP_H, "peraop_0_disp_h",
			"disp_pwm_ck"/* parent */, 16),
	GATE_PERAO0(CLK_PERAOP_0_SPI0_BCLK, "peraop_0_spi0_bclk",
			"spi_ck"/* parent */, 17),
	GATE_PERAO0(CLK_PERAOP_0_SPI1_BCLK, "peraop_0_spi1_bclk",
			"spi_ck"/* parent */, 18),
	GATE_PERAO0(CLK_PERAOP_0_SPI2_BCLK, "peraop_0_spi2_bclk",
			"spi_ck"/* parent */, 19),
	GATE_PERAO0(CLK_PERAOP_0_SPI3_BCLK, "peraop_0_spi3_bclk",
			"spi_ck"/* parent */, 20),
	GATE_PERAO0(CLK_PERAOP_0_SPI4_BCLK, "peraop_0_spi4_bclk",
			"spi_ck"/* parent */, 21),
	GATE_PERAO0(CLK_PERAOP_0_SPI5_BCLK, "peraop_0_spi5_bclk",
			"spi_ck"/* parent */, 22),
	GATE_PERAO0(CLK_PERAOP_0_SPI6_BCLK, "peraop_0_spi6_bclk",
			"spi_ck"/* parent */, 23),
	GATE_PERAO0(CLK_PERAOP_0_SPI7_BCLK, "peraop_0_spi7_bclk",
			"spi_ck"/* parent */, 24),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_1_DMA_BCLK, "peraop_1_dma_bclk",
			"peri_axi_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAOP_1_USB_FRMC, "peraop_1_usb_frmc",
			"ssusb_fmcnt_ck"/* parent */, 8),
	GATE_PERAO1(CLK_PERAOP_1_USB_SYS, "peraop_1_usb_sys",
			"usb_ck"/* parent */, 10),
	GATE_PERAO1(CLK_PERAOP_1_USB_XHCI, "peraop_1_usb_xhci",
			"ssusb_xhci_ck"/* parent */, 11),
	GATE_PERAO1(CLK_PERAOP_1_USB1P_FRMC, "peraop_1_usb1p_frmc",
			"ssusb_fmcnt_ck"/* parent */, 15),
	GATE_PERAO1(CLK_PERAOP_1_USB1P_SYS, "peraop_1_usb1p_sys",
			"usb_ck"/* parent */, 17),
	GATE_PERAO1(CLK_PERAOP_1_USB1P_XHCI, "peraop_1_usb1p_xhci",
			"ssusb_xhci_ck"/* parent */, 18),
	GATE_PERAO1(CLK_PERAOP_1_MSDC1, "peraop_1_msdc1",
			"msdc30_1_ck"/* parent */, 21),
	GATE_PERAO1(CLK_PERAOP_1_MSDC1_HCLK, "peraop_1_msdc1_hclk",
			"peri_axi_ck"/* parent */, 22),
	GATE_PERAO1(CLK_PERAOP_1_MSDC2, "peraop_1_msdc2",
			"msdc30_2_ck"/* parent */, 23),
	GATE_PERAO1(CLK_PERAOP_1_MSDC2_HCLK, "peraop_1_msdc2_hclk",
			"peri_axi_ck"/* parent */, 24),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};



static const struct mtk_gate_regs usb_sif0_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x30,
	.sta_ofs = 0x30,
};

static const struct mtk_gate_regs usb_sif1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x50,
	.sta_ofs = 0x50,
};

#define GATE_USB_SIF0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_sif0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_USB_SIF1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_sif1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate usb_sif_clks[] = {
	/* USB_SIF0 */
	GATE_USB_SIF0(CLK_USB_SIF_USB_U3_P, "usb_sif_usb_u3_p",
			"usb_ck"/* parent */, 0),
	/* USB_SIF1 */
	GATE_USB_SIF1(CLK_USB_SIF_USB_U2_P, "usb_sif_usb_u2_p",
			"usb_ck"/* parent */, 0),
};

static const struct mtk_clk_desc usb_sif_mcd = {
	.clks = usb_sif_clks,
	.num_clks = CLK_USB_SIF_NR_CLK,
};

static const struct mtk_gate_regs usb_sif_p10_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x30,
	.sta_ofs = 0x30,
};

static const struct mtk_gate_regs usb_sif_p11_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x50,
	.sta_ofs = 0x50,
};

#define GATE_USB_SIF_P10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_sif_p10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_USB_SIF_P11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_sif_p11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate usb_sif_p1_clks[] = {
	/* USB_SIF_P10 */
	GATE_USB_SIF_P10(CLK_USB_SIF_P1_USB_U3_P, "usb_sif_p1_usb_u3_p",
			"usb_ck"/* parent */, 0),
	/* USB_SIF_P11 */
	GATE_USB_SIF_P11(CLK_USB_SIF_P1_USB_U2_P, "usb_sif_p1_usb_u2_p",
			"usb_ck"/* parent */, 0),
};

static const struct mtk_clk_desc usb_sif_p1_mcd = {
	.clks = usb_sif_p1_clks,
	.num_clks = CLK_USB_SIF_P1_NR_CLK,
};

static const struct mtk_gate_regs ufsao_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
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
	GATE_UFSAO(CLK_UFSAO_U_AO_0_TXSYMBLOCLK, "ufsao_u_ao_0_tx_clk",
			"f26m_ck"/* parent */, 0),
	GATE_UFSAO(CLK_UFSAO_U_AO_0_RXSYMBLOCLK0, "ufsao_u_ao_0_rx_clk0",
			"f26m_ck"/* parent */, 1),
	GATE_UFSAO(CLK_UFSAO_U_AO_0_RXSYMBLOCLK1, "ufsao_u_ao_0_rx_clk1",
			"f26m_ck"/* parent */, 2),
	GATE_UFSAO(CLK_UFSAO_U_AO_0_SYSCLK, "ufsao_u_ao_0_sysclk",
			"ufs_haxi_ck"/* parent */, 3),
	GATE_UFSAO(CLK_UFSAO_U_AO_0_UMPSAP, "ufsao_u_ao_0_umpsap",
			"f26m_ck"/* parent */, 4),
	GATE_UFSAO(CLK_UFSAO_U_AO_0_MMPSAP, "ufsao_u_ao_0_mmpsap",
			"f26m_ck"/* parent */, 8),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct mtk_gate_regs ufspdn_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
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
	GATE_UFSPDN(CLK_UFSPDN_U_0_HCIUFS, "ufspdn_u_0_hciufs",
			"ufs_haxi_ck"/* parent */, 0),
	GATE_UFSPDN(CLK_UFSPDN_U_0_HCIAES, "ufspdn_u_0_hciaes",
			"aes_ufsfde_ck"/* parent */, 1),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = CLK_UFSPDN_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6895_peri[] = {
	{
		.compatible = "mediatek,mt6895-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6895-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6895-imp_iic_wrap_w",
		.data = &impw_mcd,
	}, {
		.compatible = "mediatek,mt6895-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6895-ssusb_sifslv_ippc",
		.data = &usb_sif_mcd,
	}, {
		.compatible = "mediatek,mt6895-ssusb_sifslv_ippc_p1",
		.data = &usb_sif_p1_mcd,
	}, {
		.compatible = "mediatek,mt6895-ufs_ao_config",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6895-ufs_pdn_cfg",
		.data = &ufspdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6895_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6895_peri_drv = {
	.probe = clk_mt6895_peri_grp_probe,
	.driver = {
		.name = "clk-mt6895-peri",
		.of_match_table = of_match_clk_mt6895_peri,
	},
};

static int __init clk_mt6895_peri_init(void)
{
	return platform_driver_register(&clk_mt6895_peri_drv);
}

static void __exit clk_mt6895_peri_exit(void)
{
	platform_driver_unregister(&clk_mt6895_peri_drv);
}

arch_initcall(clk_mt6895_peri_init);
module_exit(clk_mt6895_peri_exit);
MODULE_LICENSE("GPL");
