// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mm2_cg_regs = {
	.set_ofs = 0x1a4,
	.clr_ofs = 0x1a8,
	.sta_ofs = 0x1a0,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"disp_ck"/* parent */, 0),
	GATE_MM0(CLK_MM_DISP_RSZ1, "mm_disp_rsz1",
			"disp_ck"/* parent */, 1),
	GATE_MM0(CLK_MM_DISP_OVL0, "mm_disp_ovl0",
			"disp_ck"/* parent */, 2),
	GATE_MM0(CLK_MM_INLINE, "mm_inline",
			"disp_ck"/* parent */, 3),
	GATE_MM0(CLK_MM_MDP_TDSHP4, "mm_mdp_tdshp4",
			"disp_ck"/* parent */, 4),
	GATE_MM0(CLK_MM_MDP_TDSHP5, "mm_mdp_tdshp5",
			"disp_ck"/* parent */, 5),
	GATE_MM0(CLK_MM_MDP_AAL4, "mm_mdp_aal4",
			"disp_ck"/* parent */, 6),
	GATE_MM0(CLK_MM_MDP_AAL5, "mm_mdp_aal5",
			"disp_ck"/* parent */, 7),
	GATE_MM0(CLK_MM_MDP_HDR4, "mm_mdp_hdr4",
			"disp_ck"/* parent */, 8),
	GATE_MM0(CLK_MM_MDP_HDR5, "mm_mdp_hdr5",
			"disp_ck"/* parent */, 9),
	GATE_MM0(CLK_MM_MDP_RSZ4, "mm_mdp_rsz4",
			"disp_ck"/* parent */, 10),
	GATE_MM0(CLK_MM_MDP_RSZ5, "mm_mdp_rsz5",
			"disp_ck"/* parent */, 11),
	GATE_MM0(CLK_MM_MDP_RDMA4, "mm_mdp_rdma4",
			"disp_ck"/* parent */, 12),
	GATE_MM0(CLK_MM_MDP_RDMA5, "mm_mdp_rdma5",
			"disp_ck"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"disp_ck"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1",
			"disp_ck"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l",
			"disp_ck"/* parent */, 16),
	GATE_MM0(CLK_MM_DISP_OVL1_2L, "mm_disp_ovl1_2l",
			"disp_ck"/* parent */, 17),
	GATE_MM0(CLK_MM_DISP_OVL2_2L, "mm_disp_ovl2_2l",
			"disp_ck"/* parent */, 18),
	GATE_MM0(CLK_MM_DISP_MUTEX, "mm_disp_mutex",
			"disp_ck"/* parent */, 19),
	GATE_MM0(CLK_MM_DISP_OVL1, "mm_disp_ovl1",
			"disp_ck"/* parent */, 20),
	GATE_MM0(CLK_MM_DISP_OVL3_2L, "mm_disp_ovl3_2l",
			"disp_ck"/* parent */, 21),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp_ck"/* parent */, 22),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp_ck"/* parent */, 23),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp_ck"/* parent */, 24),
	GATE_MM0(CLK_MM_DISP_COLOR1, "mm_disp_color1",
			"disp_ck"/* parent */, 25),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"disp_ck"/* parent */, 26),
	GATE_MM0(CLK_MM_DISP_POSTMASK1, "mm_disp_postmask1",
			"disp_ck"/* parent */, 27),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp_ck"/* parent */, 28),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"disp_ck"/* parent */, 29),
	GATE_MM0(CLK_MM_DSI0_MM_CLK, "mm_dsi0_mm_clk",
			"disp_ck"/* parent */, 30),
	GATE_MM0(CLK_MM_DSI1_MM_CLK, "mm_dsi1_mm_clk",
			"disp_ck"/* parent */, 31),
	/* MM1 */
	GATE_MM1(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp_ck"/* parent */, 0),
	GATE_MM1(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
			"disp_ck"/* parent */, 1),
	GATE_MM1(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp_ck"/* parent */, 2),
	GATE_MM1(CLK_MM_DISP_AAL1, "mm_disp_aal1",
			"disp_ck"/* parent */, 3),
	GATE_MM1(CLK_MM_DISP_WDMA0, "mm_disp_wdma0",
			"disp_ck"/* parent */, 4),
	GATE_MM1(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"disp_ck"/* parent */, 5),
	GATE_MM1(CLK_MM_DISP_UFBC_WDMA0, "mm_disp_ufbc_wdma0",
			"disp_ck"/* parent */, 6),
	GATE_MM1(CLK_MM_DISP_UFBC_WDMA1, "mm_disp_ufbc_wdma1",
			"disp_ck"/* parent */, 7),
	GATE_MM1(CLK_MM_DISP_RDMA0, "mm_disp_rdma0",
			"disp_ck"/* parent */, 8),
	GATE_MM1(CLK_MM_DISP_RDMA1, "mm_disp_rdma1",
			"disp_ck"/* parent */, 9),
	GATE_MM1(CLK_MM_DISP_RDMA4, "mm_disp_rdma4",
			"disp_ck"/* parent */, 10),
	GATE_MM1(CLK_MM_DISP_RDMA5, "mm_disp_rdma5",
			"disp_ck"/* parent */, 11),
	GATE_MM1(CLK_MM_DISP_DSC_WRAP, "mm_disp_dsc_wrap",
			"disp_ck"/* parent */, 12),
	GATE_MM1(CLK_MM_DP_INTF_MM_CLK, "mm_dp_intf_mm_clk",
			"disp_ck"/* parent */, 13),
	GATE_MM1(CLK_MM_DISP_MERGE0, "mm_disp_merge0",
			"disp_ck"/* parent */, 14),
	GATE_MM1(CLK_MM_DISP_MERGE1, "mm_disp_merge1",
			"disp_ck"/* parent */, 15),
	GATE_MM1(CLK_MM_SMI_COMMON, "mm_smi_common",
			"disp_ck"/* parent */, 19),
	GATE_MM1(CLK_MM_SMI_GALS, "mm_smi_gals",
			"disp_ck"/* parent */, 23),
	GATE_MM1(CLK_MM_SMI_INFRA, "mm_smi_infra",
			"disp_ck"/* parent */, 27),
	GATE_MM1(CLK_MM_SMI_IOMMU, "mm_smi_iommu",
			"disp_ck"/* parent */, 31),
	/* MM2 */
	GATE_MM2(CLK_MM_DSI0_INTF_CLK, "mm_dsi0_intf_clk",
			"disp_ck"/* parent */, 0),
	GATE_MM2(CLK_MM_DSI1_INTF_CLK, "mm_dsi1_intf_clk",
			"disp_ck"/* parent */, 8),
	GATE_MM2(CLK_MM_DP_INTF_INTF_CLK, "mm_dp_intf_intf_clk",
			"dp_ck"/* parent */, 16),
	GATE_MM2(CLK_MM_CK_26_MHZ, "mm_26_mhz",
			"f26m_ck"/* parent */, 24),
	GATE_MM2(CLK_MM_CK_32_KHZ, "mm_32_khz",
			"frtc_ck"/* parent */, 25),
};

static int clk_mt6893_mm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_MM_NR_CLK);

	mtk_clk_register_gates_with_dev(node, mm_clks, ARRAY_SIZE(mm_clks),
			clk_data, &pdev->dev);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6893_mm[] = {
	{ .compatible = "mediatek,mt6893-mmsys", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_mm_drv = {
	.probe = clk_mt6893_mm_probe,
	.driver = {
		.name = "clk-mt6893-mm",
		.of_match_table = of_match_clk_mt6893_mm,
	},
};

builtin_platform_driver(clk_mt6893_mm_drv);

#else

static struct platform_driver clk_mt6893_mm_drv = {
	.probe = clk_mt6893_mm_probe,
	.driver = {
		.name = "clk-mt6893-mm",
		.of_match_table = of_match_clk_mt6893_mm,
	},
};

static int __init clk_mt6893_mm_init(void)
{
	return platform_driver_register(&clk_mt6893_mm_drv);
}

static void __exit clk_mt6893_mm_exit(void)
{
	platform_driver_unregister(&clk_mt6893_mm_drv);
}

arch_initcall(clk_mt6893_mm_init);
module_exit(clk_mt6893_mm_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
