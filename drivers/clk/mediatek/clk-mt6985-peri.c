// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6985-clk.h>

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
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impn_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impn_clks[] = {
	GATE_IMPN(CLK_IMPN_I2C0, "impn_i2c0",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPN(CLK_IMPN_I2C6, "impn_i2c6",
			"i2c_pseudo_ck"/* parent */, 1),
	GATE_IMPN(CLK_IMPN_I2C10, "impn_i2c10",
			"i2c_pseudo_ck"/* parent */, 2),
	GATE_IMPN(CLK_IMPN_I2C11, "impn_i2c11",
			"i2c_pseudo_ck"/* parent */, 3),
	GATE_IMPN(CLK_IMPN_I2C12, "impn_i2c12",
			"i2c_pseudo_ck"/* parent */, 4),
	GATE_IMPN(CLK_IMPN_I2C13, "impn_i2c13",
			"i2c_pseudo_ck"/* parent */, 5),
};

static const struct mtk_clk_desc impn_mcd = {
	.clks = impn_clks,
	.num_clks = CLK_IMPN_NR_CLK,
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
	GATE_IMPS(CLK_IMPS_I2C1, "imps_i2c1",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_I2C2, "imps_i2c2",
			"i2c_pseudo_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_I2C3, "imps_i2c3",
			"i2c_pseudo_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_I2C4, "imps_i2c4",
			"i2c_pseudo_ck"/* parent */, 3),
	GATE_IMPS(CLK_IMPS_I2C7, "imps_i2c7",
			"i2c_pseudo_ck"/* parent */, 4),
	GATE_IMPS(CLK_IMPS_I2C8, "imps_i2c8",
			"i2c_pseudo_ck"/* parent */, 5),
	GATE_IMPS(CLK_IMPS_I2C9, "imps_i2c9",
			"i2c_pseudo_ck"/* parent */, 6),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
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
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
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
	GATE_PERAO0(CLK_PERAO_UART2, "perao_uart2",
			"uart_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAO_UART3, "perao_uart3",
			"uart_ck"/* parent */, 3),
	GATE_PERAO0(CLK_PERAO_PWM_H, "perao_pwm_h",
			"peri_faxi_ck"/* parent */, 4),
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
	GATE_PERAO0(CLK_PERAO_DISP_PWM0, "perao_disp_pwm0",
			"disp_pwm_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAO_DISP_PWM1, "perao_disp_pwm1",
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
	GATE_PERAO0(CLK_PERAO_SFLASH, "perao_sflash",
			"sflash_ck"/* parent */, 28),
	GATE_PERAO0(CLK_PERAO_SFLASH_F, "perao_sflash_f",
			"peri_faxi_ck"/* parent */, 29),
	GATE_PERAO0(CLK_PERAO_SFLASH_H, "perao_sflash_h",
			"peri_faxi_ck"/* parent */, 30),
	GATE_PERAO0(CLK_PERAO_SFLASH_P, "perao_sflash_p",
			"peri_faxi_ck"/* parent */, 31),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAO_DMA_B, "perao_dma_b",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAO_SSUSB0_FRMCNT, "perao_ssusb0_frmcnt",
			"clk26m"/* parent */, 4),
	GATE_PERAO1(CLK_PERAO_SSUSB1_FRMCNT, "perao_ssusb1_frmcnt",
			"clk26m"/* parent */, 11),
	GATE_PERAO1(CLK_PERAO_MSDC1, "perao_msdc1",
			"msdc30_1_ck"/* parent */, 17),
	GATE_PERAO1(CLK_PERAO_MSDC1_F, "perao_msdc1_f",
			"peri_faxi_ck"/* parent */, 18),
	GATE_PERAO1(CLK_PERAO_MSDC1_H, "perao_msdc1_h",
			"peri_faxi_ck"/* parent */, 19),
	GATE_PERAO1(CLK_PERAO_MSDC2, "perao_msdc2",
			"msdc30_2_ck"/* parent */, 20),
	GATE_PERAO1(CLK_PERAO_MSDC2_F, "perao_msdc2_f",
			"peri_faxi_ck"/* parent */, 21),
	GATE_PERAO1(CLK_PERAO_MSDC2_H, "perao_msdc2_h",
			"peri_faxi_ck"/* parent */, 22),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAO_AUDIO_SLV, "perao_audio_slv",
			"peri_faxi_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAO_AUDIO_MST, "perao_audio_mst",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO2(CLK_PERAO_AUDIO_INTBUS, "perao_audio_intbus",
			"aud_intbus_ck"/* parent */, 2),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs pext_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXT(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pext_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate pext_clks[] = {
	GATE_PEXT(CLK_PEXT_MAC0_26M, "pext_mac0_26m",
			"f26m_ck"/* parent */, 0),
	GATE_PEXT(CLK_PEXT_MAC0_P1_PCLK_250M, "pext_mac0_p1_p_250m",
			"f26m_ck"/* parent */, 1),
	GATE_PEXT(CLK_PEXT_MAC0_GFMUX_TL, "pext_mac0_gfmux_tl",
			"tl_ck"/* parent */, 2),
	GATE_PEXT(CLK_PEXT_MAC0_FMEM, "pext_mac0_fmem",
			"pextp_fmem_sub_ck"/* parent */, 3),
	GATE_PEXT(CLK_PEXT_MAC0_HCLK, "pext_mac0",
			"pextp_faxi_ck"/* parent */, 4),
	GATE_PEXT(CLK_PEXT_PHY0_REF, "pext_phy0_ref",
			"f26m_ck"/* parent */, 5),
	GATE_PEXT(CLK_PEXT_MAC1_26M, "pext_mac1_26m",
			"f26m_ck"/* parent */, 8),
	GATE_PEXT(CLK_PEXT_MAC1_P1_PCLK_250M, "pext_mac1_p1_p_250m",
			"f26m_ck"/* parent */, 9),
	GATE_PEXT(CLK_PEXT_MAC1_GFMUX_TL, "pext_mac1_gfmux_tl",
			"tl_ck"/* parent */, 10),
	GATE_PEXT(CLK_PEXT_MAC1_FMEM, "pext_mac1_fmem",
			"pextp_fmem_sub_ck"/* parent */, 11),
	GATE_PEXT(CLK_PEXT_MAC1_HCLK, "pext_mac1",
			"pextp_faxi_ck"/* parent */, 12),
	GATE_PEXT(CLK_PEXT_PHY1_REF, "pext_phy1_ref",
			"f26m_ck"/* parent */, 13),
};

static const struct mtk_clk_desc pext_mcd = {
	.clks = pext_clks,
	.num_clks = CLK_PEXT_NR_CLK,
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

static const struct of_device_id of_match_clk_mt6985_peri[] = {
	{
		.compatible = "mediatek,mt6985-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6985-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6985-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6985-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6985-pextpcfg_ao",
		.data = &pext_mcd,
	}, {
		.compatible = "mediatek,mt6985-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6985-ufscfg_pdn",
		.data = &ufspdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6985_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6985_peri_drv = {
	.probe = clk_mt6985_peri_grp_probe,
	.driver = {
		.name = "clk-mt6985-peri",
		.of_match_table = of_match_clk_mt6985_peri,
	},
};

module_platform_driver(clk_mt6985_peri_drv);
MODULE_LICENSE("GPL");
