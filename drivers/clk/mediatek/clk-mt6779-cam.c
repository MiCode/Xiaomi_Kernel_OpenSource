// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/mt6779-clk.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#define MT_CLKMGR_MODULE_INIT	0
#define CCF_SUBSYS_DEBUG		1

static const struct mtk_gate_regs cam_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_CAM(_id, _name, _parent, _shift) {	\
	.id = _id,				\
	.name = _name,				\
	.parent_name = _parent,			\
	.regs = &cam_cg_regs,			\
	.shift = _shift,			\
	.ops = &mtk_clk_gate_ops_setclr,	\
}

#define GATE_CAM_DUMMY(_id, _name, _parent, _shift) {	\
	.id = _id,				\
	.name = _name,				\
	.parent_name = _parent,			\
	.regs = &cam_cg_regs,			\
	.shift = _shift,			\
	.ops = &mtk_clk_gate_ops_setclr_dummy,	\
}


static const struct mtk_gate cam_clks[] = {
	GATE_CAM_DUMMY(CLK_CAM_LARB10, "camsys_larb10", "cam_sel", 0),
	GATE_CAM(CLK_CAM_DFP_VAD, "camsys_dfp_vad", "cam_sel", 1),
	GATE_CAM_DUMMY(CLK_CAM_LARB11, "camsys_larb11", "cam_sel", 2),
	GATE_CAM_DUMMY(CLK_CAM_LARB9, "camsys_larb9", "cam_sel", 3),
	GATE_CAM(CLK_CAM_CAM, "camsys_cam", "cam_sel", 6),
	GATE_CAM(CLK_CAM_CAMTG, "camsys_camtg", "cam_sel", 7),
	GATE_CAM_DUMMY(CLK_CAM_SENINF, "camsys_seninf", "cam_sel", 8),
	GATE_CAM_DUMMY(CLK_CAM_CAMSV0, "camsys_camsv0", "cam_sel", 9),
	GATE_CAM_DUMMY(CLK_CAM_CAMSV1, "camsys_camsv1", "cam_sel", 10),
	GATE_CAM_DUMMY(CLK_CAM_CAMSV2, "camsys_camsv2", "cam_sel", 11),
	GATE_CAM_DUMMY(CLK_CAM_CAMSV3, "camsys_camsv3", "cam_sel", 12),
	GATE_CAM_DUMMY(CLK_CAM_CCU, "camsys_ccu", "cam_sel", 13),
	GATE_CAM(CLK_CAM_FAKE_ENG, "camsys_fake_eng", "cam_sel", 14),
};

static const struct of_device_id of_match_clk_mt6779_cam[] = {
	{ .compatible = "mediatek,mt6779-camsys", },
	{}
};

static int clk_mt6779_cam_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_CAM_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

#if CCF_SUBSYS_DEBUG
	pr_info("%s(): clk data number: %d\n", __func__, clk_data->clk_num);
#endif

	mtk_clk_register_gates(node, cam_clks, ARRAY_SIZE(cam_clks),
			       clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret) {
		pr_notice("%s(): could not register clock provider: %d\n",
					__func__, ret);

		kfree(clk_data);
	}

	return ret;

}

static struct platform_driver clk_mt6779_cam_drv = {
	.probe = clk_mt6779_cam_probe,
	.driver = {
		.name = "clk-mt6779-cam",
		.of_match_table = of_match_clk_mt6779_cam,
	},
};

#if MT_CLKMGR_MODULE_INIT

builtin_platform_driver(clk_mt6779_cam_drv);

#else

static int __init clk_mt6779_cam_platform_init(void)
{
	return platform_driver_register(&clk_mt6779_cam_drv);
}

arch_initcall_sync(clk_mt6779_cam_platform_init);

#endif /* MT_CLKMGR_MODULE_INIT */

