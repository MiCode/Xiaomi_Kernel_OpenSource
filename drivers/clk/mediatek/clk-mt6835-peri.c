// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6835-clk.h>

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
			"i2c_pseudo10_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_AP_CLOCK_I2C11, "impc_ap_clock_i2c11",
			"i2c_pseudo11_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
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
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C0, "impen_ap_clock_i2c0",
			"i2c_pseudo0_ck"/* parent */, 0),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C2, "impen_ap_clock_i2c2",
			"i2c_pseudo2_ck"/* parent */, 1),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C4, "impen_ap_clock_i2c4",
			"i2c_pseudo4_ck"/* parent */, 2),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C9, "impen_ap_clock_i2c9",
			"i2c_pseudo9_ck"/* parent */, 3),
};

static const struct mtk_clk_desc impen_mcd = {
	.clks = impen_clks,
	.num_clks = CLK_IMPEN_NR_CLK,
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
			"i2c_pseudo1_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C6, "imps_ap_clock_i2c6",
			"i2c_pseudo6_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C7, "imps_ap_clock_i2c7",
			"i2c_pseudo7_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C8, "imps_ap_clock_i2c8",
			"i2c_pseudo8_ck"/* parent */, 3),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct mtk_gate_regs impws_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPWS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impws_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impws_clks[] = {
	GATE_IMPWS(CLK_IMPWS_AP_CLOCK_I2C3, "impws_ap_clock_i2c3",
			"i2c_pseudo3_ck"/* parent */, 0),
	GATE_IMPWS(CLK_IMPWS_AP_CLOCK_I2C5, "impws_ap_clock_i2c5",
			"i2c_pseudo5_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impws_mcd = {
	.clks = impws_clks,
	.num_clks = CLK_IMPWS_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x28,
	.clr_ofs = 0x2C,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x34,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x38,
	.clr_ofs = 0x3C,
	.sta_ofs = 0x18,
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
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAOP_UART0, "peraop_uart0",
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAOP_PWM_HCLK, "peraop_pwm_hclk",
			"axip_ck"/* parent */, 3),
	GATE_PERAO0(CLK_PERAOP_PWM_BCLK, "peraop_pwm_bclk",
			"uart_ck"/* parent */, 4),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK1, "peraop_pwm_fbclk1",
			"uart_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK2, "peraop_pwm_fbclk2",
			"uart_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK3, "peraop_pwm_fbclk3",
			"uart_ck"/* parent */, 7),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK4, "peraop_pwm_fbclk4",
			"uart_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAOP_BTIF_BCLK, "peraop_btif_bclk",
			"uart_ck"/* parent */, 9),
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
			"ssusb_fmcnt_ck"/* parent */, 31),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_USB_SYS, "peraop_usb_sys",
			"usb_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAOP_USB_XHCI, "peraop_usb_xhci",
			"ssusb_xhci_ck"/* parent */, 2),
	GATE_PERAO1(CLK_PERAOP_MSDC1_SRC, "peraop_msdc1_src",
			"msdc30_1_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAOP_MSDC1_HCLK, "peraop_msdc1_hclk",
			"axip_ck"/* parent */, 6),
	GATE_PERAO1(CLK_PERAOP_MSDC0_SRC, "peraop_msdc0_src",
			"msdc50_0_ck"/* parent */, 18),
	GATE_PERAO1(CLK_PERAOP_MSDC0_HCLK, "peraop_msdc0_hclk",
			"msdc5hclk_ck"/* parent */, 19),
	GATE_PERAO1(CLK_PERAOP_MSDC0_AES, "peraop_msdc0_aes",
			"aes_msdcfde_ck"/* parent */, 20),
	GATE_PERAO1(CLK_PERAOP_MSDC0_XCLK, "peraop_msdc0_xclk",
			"axip_ck"/* parent */, 21),
	GATE_PERAO1(CLK_PERAOP_MSDC0_HCLK_WRAP, "peraop_msdc0_h_wrap",
			"axip_ck"/* parent */, 22),
	GATE_PERAO1(CLK_PERAOP_NFIECC_BCLK, "peraop_nfiecc_bclk",
			"nfi1x_ck"/* parent */, 27),
	GATE_PERAO1(CLK_PERAOP_NFI_BCLK, "peraop_nfi_bclk",
			"nfi1x_ck"/* parent */, 28),
	GATE_PERAO1(CLK_PERAOP_NFI_HCLK, "peraop_nfi_hclk",
			"axip_ck"/* parent */, 29),
	GATE_PERAO1(CLK_AUXADC_BCLK_AP, "auxadc_bclk_ap",
			"f26m_ck"/* parent */, 30),
	GATE_PERAO1(CLK_AUXADC_BCLK_MD, "auxadc_bclk_md",
			"f26m_ck"/* parent */, 31),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAO_AUDIO_SLV_CKP, "perao_audio_slv_ckp",
			"axip_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAO_AUDIO_MST_CKP, "perao_audio_mst_ckp",
			"axip_ck"/* parent */, 1),
	GATE_PERAO2(CLK_PERAO_INTBUS_CKP, "perao_intbus_ckp",
			"aud_intbus_ck"/* parent */, 2),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};





static const struct of_device_id of_match_clk_mt6835_peri[] = {
	{
		.compatible = "mediatek,mt6835-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6835-imp_iic_wrap_en",
		.data = &impen_mcd,
	}, {
		.compatible = "mediatek,mt6835-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6835-imp_iic_wrap_ws",
		.data = &impws_mcd,
	}, {
		.compatible = "mediatek,mt6835-pericfg_ao",
		.data = &perao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6835_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6835_peri_drv = {
	.probe = clk_mt6835_peri_grp_probe,
	.driver = {
		.name = "clk-mt6835-peri",
		.of_match_table = of_match_clk_mt6835_peri,
	},
};

module_platform_driver(clk_mt6835_peri_drv);
MODULE_LICENSE("GPL");
