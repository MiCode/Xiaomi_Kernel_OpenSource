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

#include <dt-bindings/clock/mt6855-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs imp0_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

static const struct mtk_gate_regs imp1_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

static const struct mtk_gate_regs imp2_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

static const struct mtk_gate_regs imp3_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
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

static const struct mtk_gate imp0_clks[] = {
	/* IMP0 */
	GATE_IMP0(CLK_IMP_AP_CLOCK_I2C11, "imp_ap_clock_i2c11",
			"i2c_ck"/* parent */, 0),
};

static const struct mtk_gate imp3_clks[] = {
	/* IMP1 */
	GATE_IMP3(CLK_IMP_AP_CLOCK_I2C5, "imp_ap_clock_i2c5",
			"i2c_ck"/* parent */, 0),
};

static const struct mtk_gate imp2_clks[] = {
	/* IMP2 */
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C3, "imp_ap_clock_i2c3",
			"i2c_ck"/* parent */, 0),
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C6, "imp_ap_clock_i2c6",
			"i2c_ck"/* parent */, 1),
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C7, "imp_ap_clock_i2c7",
			"i2c_ck"/* parent */, 2),
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C8, "imp_ap_clock_i2c8",
			"i2c_ck"/* parent */, 3),
	GATE_IMP2(CLK_IMP_AP_CLOCK_I2C10, "imp_ap_clock_i2c10",
			"i2c_ck"/* parent */, 4),
};

static const struct mtk_gate imp1_clks[] = {
	/* IMP3 */
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C0, "imp_ap_clock_i2c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C1, "imp_ap_clock_i2c1",
			"i2c_ck"/* parent */, 1),
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C2, "imp_ap_clock_i2c2",
			"i2c_ck"/* parent */, 2),
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C4, "imp_ap_clock_i2c4",
			"i2c_ck"/* parent */, 3),
	GATE_IMP1(CLK_IMP_AP_CLOCK_I2C9, "imp_ap_clock_i2c9",
			"i2c_ck"/* parent */, 4),
};

static const struct mtk_clk_desc imp_mcd0 = {
	.clks = imp0_clks,
	.num_clks = CLK_IMP0_NR_CLK,
};

static const struct mtk_clk_desc imp_mcd1 = {
	.clks = imp1_clks,
	.num_clks = CLK_IMP1_NR_CLK,
};

static const struct mtk_clk_desc imp_mcd2 = {
	.clks = imp2_clks,
	.num_clks = CLK_IMP2_NR_CLK,
};

static const struct mtk_clk_desc imp_mcd3 = {
	.clks = imp3_clks,
	.num_clks = CLK_IMP3_NR_CLK,
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
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"uart_ck"/* parent */, 1),
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
			"pwm_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAOP_DISP_PWM0, "peraop_disp_pwm0",
			"disp_pwm_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAOP_SPI0_BCLK, "peraop_spi0_bclk",
			"spi_sel"/* parent */, 11),
	GATE_PERAO0(CLK_PERAOP_SPI1_BCLK, "peraop_spi1_bclk",
			"spi_sel"/* parent */, 12),
	GATE_PERAO0(CLK_PERAOP_SPI2_BCLK, "peraop_spi2_bclk",
			"spi_sel"/* parent */, 13),
	GATE_PERAO0(CLK_PERAOP_SPI3_BCLK, "peraop_spi3_bclk",
			"spi_sel"/* parent */, 14),
	GATE_PERAO0(CLK_PERAOP_SPI4_BCLK, "peraop_spi4_bclk",
			"spi_sel"/* parent */, 15),
	GATE_PERAO0(CLK_PERAOP_SPI5_BCLK, "peraop_spi5_bclk",
			"spi_sel"/* parent */, 16),
	GATE_PERAO0(CLK_PERAOP_SPI6_BCLK, "peraop_spi6_bclk",
			"spi_sel"/* parent */, 17),
	GATE_PERAO0(CLK_PERAOP_SPI7_BCLK, "peraop_spi7_bclk",
			"spi_sel"/* parent */, 18),
	GATE_PERAO0(CLK_PERAOP_APDMA, "peraop_apdma",
			"axip_ck"/* parent */, 28),
	GATE_PERAO0(CLK_PERAOP_USB_PHY, "peraop_usb_phy",
			"f26m_ck"/* parent */, 30),
	GATE_PERAO0(CLK_PERAOP_USB_SYS, "peraop_usb_sys",
			"usb_ck"/* parent */, 31),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_USB_DMA_BUS, "peraop_usb_dma_bus",
			"axip_ck"/* parent */, 0),
	GATE_PERAO1(CLK_PERAOP_USB_MCU_BUS, "peraop_usb_mcu_bus",
			"axip_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAOP_MSDC1, "peraop_msdc1",
			"msdc30_1_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAOP_MSDC1_HCLK, "peraop_msdc1_hclk",
			"axip_ck"/* parent */, 6),
	GATE_PERAO1(CLK_PERAOP_FMSDC50, "peraop_fmsdc50",
			"msdc50_0_ck"/* parent */, 18),
	GATE_PERAO1(CLK_PERAOP_FMSDC50_HCLK, "peraop_fmsdc50_hclk",
			"msdc5hclk_ck"/* parent */, 19),
	GATE_PERAO1(CLK_PERAOP_FAES_MSDCFDE, "peraop_faes_msdcfde",
			"aes_msdcfde_ck"/* parent */, 20),
	GATE_PERAO1(CLK_PERAOP_MSDC50_XCLK, "peraop_msdc50_xclk",
			"axip_ck"/* parent */, 21),
	GATE_PERAO1(CLK_PERAOP_MSDC50_HCLK, "peraop_msdc50_hclk",
			"axip_ck"/* parent */, 22),
	GATE_PERAO1(CLK_PERAO_AUDIO_SLV_CK_PERI, "perao_audio_slv_peri",
			"f26m_ck"/* parent */, 29),
	GATE_PERAO1(CLK_PERAO_AUDIO_MST_CK_PERI, "perao_audio_mst_peri",
			"f26m_ck"/* parent */, 30),
	GATE_PERAO1(CLK_PERAO_INTBUS_CK_PERI, "perao_intbus_peri",
			"f26m_ck"/* parent */, 31),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6855_peri[] = {
	{
		.compatible = "mediatek,mt6855-imp_iic_wrap0",
		.data = &imp_mcd0,
	}, {
		.compatible = "mediatek,mt6855-imp_iic_wrap1",
		.data = &imp_mcd1,
	}, {
		.compatible = "mediatek,mt6855-imp_iic_wrap2",
		.data = &imp_mcd2,
	}, {
		.compatible = "mediatek,mt6855-imp_iic_wrap3",
		.data = &imp_mcd3,
	}, {
		.compatible = "mediatek,mt6855-pericfg_ao",
		.data = &perao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6855_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6855_peri_drv = {
	.probe = clk_mt6855_peri_grp_probe,
	.driver = {
		.name = "clk-mt6855-peri",
		.of_match_table = of_match_clk_mt6855_peri,
	},
};

static int __init clk_mt6855_peri_init(void)
{
	return platform_driver_register(&clk_mt6855_peri_drv);
}

static void __exit clk_mt6855_peri_exit(void)
{
	platform_driver_unregister(&clk_mt6855_peri_drv);
}

arch_initcall(clk_mt6855_peri_init);
module_exit(clk_mt6855_peri_exit);
MODULE_LICENSE("GPL");
