// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6886-clk.h>

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
	GATE_IMPC(CLK_IMPC_I2C5, "impc_i2c5",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_I2C6, "impc_i2c6",
			"i2c_pseudo_ck"/* parent */, 1),
	GATE_IMPC(CLK_IMPC_I2C10, "impc_i2c10",
			"i2c_pseudo_ck"/* parent */, 2),
	GATE_IMPC(CLK_IMPC_I2C11, "impc_i2c11",
			"i2c_pseudo_ck"/* parent */, 3),
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
	GATE_IMPE(CLK_IMPE_I2C3, "impe_i2c3",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPE(CLK_IMPE_I2C7, "impe_i2c7",
			"i2c_pseudo_ck"/* parent */, 1),
	GATE_IMPE(CLK_IMPE_I2C8, "impe_i2c8",
			"i2c_pseudo_ck"/* parent */, 2),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
};

static const struct mtk_gate_regs impes_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPES(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impes_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impes_clks[] = {
	GATE_IMPES(CLK_IMPES_I2C2, "impes_i2c2",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPES(CLK_IMPES_I2C4, "impes_i2c4",
			"i2c_pseudo_ck"/* parent */, 1),
	GATE_IMPES(CLK_IMPES_I2C9, "impes_i2c9",
			"i2c_pseudo_ck"/* parent */, 2),
};

static const struct mtk_clk_desc impes_mcd = {
	.clks = impes_clks,
	.num_clks = CLK_IMPES_NR_CLK,
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
	GATE_IMPW(CLK_IMPW_I2C0, "impw_i2c0",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPW(CLK_IMPW_I2C1, "impw_i2c1",
			"i2c_pseudo_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impw_mcd = {
	.clks = impw_clks,
	.num_clks = CLK_IMPW_NR_CLK,
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
	GATE_PERAO0(CLK_PERAO_UART0, "perao_uart0",
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAO_UART1, "perao_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAO_PWM_H, "perao_pwm_h",
			"peri_axi_ck"/* parent */, 4),
	GATE_PERAO0(CLK_PERAO_PWM_B, "perao_pwm_b",
			"pwm_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAO_PWM_FB1, "perao_pwm_fb1",
			"pwm_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAO_PWM_FB2, "perao_pwm_fb2",
			"pwm_ck"/* parent */, 7),
	GATE_PERAO0(CLK_PERAO_PWM_FB3, "perao_pwm_fb3",
			"pwm_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAO_PWM_FB4, "perao_pwm_fb4",
			"pwm_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAO_BTIF_B, "perao_btif_b",
			"pwm_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAO_DISP_PWM0, "perao_disp_pwm0",
			"disp_pwm_ck"/* parent */, 11),
	GATE_PERAO0(CLK_PERAO_SPI0_B, "perao_spi0_b",
			"spi_ck"/* parent */, 12),
	GATE_PERAO0(CLK_PERAO_SPI1_B, "perao_spi1_b",
			"spi_ck"/* parent */, 13),
	GATE_PERAO0(CLK_PERAO_SPI2_B, "perao_spi2_b",
			"spi_ck"/* parent */, 14),
	GATE_PERAO0(CLK_PERAO_SPI3_B, "perao_spi3_b",
			"spi_ck"/* parent */, 15),
	GATE_PERAO0(CLK_PERAO_SPI4_B, "perao_spi4_b",
			"spi_ck"/* parent */, 16),
	GATE_PERAO0(CLK_PERAO_SPI5_B, "perao_spi5_b",
			"spi_ck"/* parent */, 17),
	GATE_PERAO0(CLK_PERAO_SPI6_B, "perao_spi6_b",
			"spi_ck"/* parent */, 18),
	GATE_PERAO0(CLK_PERAO_SPI7_B, "perao_spi7_b",
			"spi_ck"/* parent */, 19),
	GATE_PERAO0(CLK_PERAO_APDMA, "perao_apdma",
			"peri_axi_ck"/* parent */, 29),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAO_USB_SYS, "perao_usb_sys",
			"usb_ck"/* parent */, 2),
	GATE_PERAO1(CLK_PERAO_USB_XHCI, "perao_usb_xhci",
			"ssusb_xhci_ck"/* parent */, 3),
	GATE_PERAO1(CLK_PERAO_USB_BUS, "perao_usb_bus",
			"peri_axi_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAO_MSDC1, "perao_msdc1",
			"msdc30_1_ck"/* parent */, 6),
	GATE_PERAO1(CLK_PERAO_MSDC1_H, "perao_msdc1_h",
			"peri_axi_ck"/* parent */, 7),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAO_AUDIO_SLV_CKP, "perao_audio_slv_ckp",
			"peri_axi_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAO_AUDIO_MST_CKP, "perao_audio_mst_ckp",
			"peri_axi_ck"/* parent */, 1),
	GATE_PERAO2(CLK_PERAO_INTBUS_CKP, "perao_intbus_ckp",
			"aud_intbus_ck"/* parent */, 2),
	GATE_PERAO2(CLK_PERAO_AUDIO_MST_IDLE_EN, "perao_aud_mst_idl_en",
			"peri_axi_ck"/* parent */, 3),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs scp_iic_cg_regs = {
	.set_ofs = 0xE18,
	.clr_ofs = 0xE14,
	.sta_ofs = 0xE10,
};

#define GATE_SCP_IIC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_iic_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate scp_iic_clks[] = {
	GATE_SCP_IIC(CLK_SCP_IIC_I2C0, "scp_iic_i2c0",
			"ulposc_ck"/* parent */, 0),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C1, "scp_iic_i2c1",
			"ulposc_ck"/* parent */, 1),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C2, "scp_iic_i2c2",
			"ulposc_ck"/* parent */, 2),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C3, "scp_iic_i2c3",
			"ulposc_ck"/* parent */, 3),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C4, "scp_iic_i2c4",
			"ulposc_ck"/* parent */, 4),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C5, "scp_iic_i2c5",
			"ulposc_ck"/* parent */, 5),
	GATE_SCP_IIC(CLK_SCP_IIC_I2C6, "scp_iic_i2c6",
			"ulposc_ck"/* parent */, 6),
};

static const struct mtk_clk_desc scp_iic_mcd = {
	.clks = scp_iic_clks,
	.num_clks = CLK_SCP_IIC_NR_CLK,
};

static const struct mtk_gate_regs ufsao_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
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
	GATE_UFSAO(CLK_UFSAO_UNIPRO_TX_SYM, "ufsao_unipro_tx_sym",
			"f26m_ck"/* parent */, 0),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM0, "ufsao_unipro_rx_sym0",
			"f26m_ck"/* parent */, 1),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM1, "ufsao_unipro_rx_sym1",
			"f26m_ck"/* parent */, 2),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SYS, "ufsao_unipro_sys",
			"ufs_ck"/* parent */, 3),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_PHY_SAP, "ufsao_unipro_phy_sap",
			"f26m_ck"/* parent */, 8),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct mtk_gate_regs ufspdn_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
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
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS, "ufspdn_ufshci_ufs",
			"ufs_ck"/* parent */, 0),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AES, "ufspdn_ufshci_aes",
			"aes_ufsfde_ck"/* parent */, 1),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = CLK_UFSPDN_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6886_peri[] = {
	{
		.compatible = "mediatek,mt6886-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6886-imp_iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt6886-imp_iic_wrap_es",
		.data = &impes_mcd,
	}, {
		.compatible = "mediatek,mt6886-imp_iic_wrap_w",
		.data = &impw_mcd,
	}, {
		.compatible = "mediatek,mt6886-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6886-scp_iic",
		.data = &scp_iic_mcd,
	}, {
		.compatible = "mediatek,mt6886-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6886-ufscfg_pdn",
		.data = &ufspdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6886_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6886_peri_drv = {
	.probe = clk_mt6886_peri_grp_probe,
	.driver = {
		.name = "clk-mt6886-peri",
		.of_match_table = of_match_clk_mt6886_peri,
	},
};

module_platform_driver(clk_mt6886_peri_drv);
MODULE_LICENSE("GPL");
