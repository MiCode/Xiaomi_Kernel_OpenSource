// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "mtk_cam_ut.h"
#include "mtk_cam_ut-engines.h"
#include "mtk_cam_regs.h"

/* seninf */
static int get_test_hmargin(int w, int h, int clk_cnt, int clk_mhz, int fps)
{
	int target_h = clk_mhz * (1000000/fps) / w * max(8/(clk_cnt+1), 1);

	return max(target_h - h, 0x80);
}

static int ut_seninf_set_testmdl(struct device *dev,
				 int width, int height,
				 int pixmode_lg2,
				 int pattern,
				 int seninf_idx,
				 int tg_idx)
{
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	void __iomem *base = seninf->base + seninf_idx * SENINF_OFFSET;
	const u16 dummy_pxl = 0x80, h_margin = 0x80;
	const u8 clk_div_cnt = (8 >> pixmode_lg2) - 1;
	const u16 dum_vsync = get_test_hmargin(width + dummy_pxl,
					      height + h_margin,
					      clk_div_cnt, 416, 30);
	unsigned int cam_mux_ctrl;
	void __iomem *cam_mux_ctrl_addr = ISP_SENINF_CAM_MUX_CTRL_0(seninf->base);

	dev_info(dev, "%s width %d x height %d dum_vsync %d pixmode_lg2 %d clk_div_cnt %d\n",
		 __func__, width, height, dum_vsync,
		 pixmode_lg2, clk_div_cnt);
	dev_info(dev, "%s seninf_idx %d tg_idx %d\n", __func__, seninf_idx, tg_idx);

	/* test mdl */
	writel_relaxed((height + h_margin) << 16 | width,
		       ISP_SENINF_TM_SIZE(base));
	/*writel_relaxed(clk_div_cnt, ISP_SENINF_TM_CLK(base));*/
	writel_relaxed(0xF, ISP_SENINF_TM_CLK(base));
	writel_relaxed(dum_vsync << 16 | dummy_pxl, ISP_SENINF_TM_DUM(base));
	writel_relaxed(pattern << 4 | 0x1, ISP_SENINF_TM_CTL(base));
	writel_relaxed(0x1f << 16 | pixmode_lg2 << 8 | 0x1,
		       ISP_SENINF_MUX_CTRL_1(base));
	writel_relaxed(0x1, ISP_SENINF_MUX_CTRL_0(base));
	writel_relaxed(0x1, ISP_SENINF_TSETMDL_CTRL(base));
	/* make sure all the seninf setting take effect */
	wmb();
	writel_relaxed(0x1, ISP_SENINF_CTRL(base));

	/* cam mux ctrl */
	switch (tg_idx / 4) {
	case 0:
		cam_mux_ctrl_addr =
			ISP_SENINF_CAM_MUX_CTRL_0(seninf->base);
		break;
	case 1:
		cam_mux_ctrl_addr =
			ISP_SENINF_CAM_MUX_CTRL_1(seninf->base);
		break;
	case 2:
		cam_mux_ctrl_addr =
			ISP_SENINF_CAM_MUX_CTRL_2(seninf->base);
		break;
	case 3:
		cam_mux_ctrl_addr =
			ISP_SENINF_CAM_MUX_CTRL_3(seninf->base);
		break;
	default:
		dev_info(dev, "unknown tg_idx:%d", tg_idx);
		break;
	}
	cam_mux_ctrl = readl_relaxed(cam_mux_ctrl_addr);
	cam_mux_ctrl &= ~(0xFF << ((tg_idx % 4) * 8));
	cam_mux_ctrl |= seninf_idx << ((tg_idx % 4) * 8);

	writel_relaxed(0x80, ISP_SENINF_CAM_MUX_CTRL_0(seninf->base));

	return 0;
}

static void ut_seninf_set_ops(struct device *dev)
{
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);

	seninf->ops.set_size = ut_seninf_set_testmdl;
}

static int mtk_ut_seninf_component_bind(struct device *dev,
					struct device *master,
					void *data)
{
	struct mtk_cam_ut *ut = data;

	dev_dbg(dev, "%s\n", __func__);
	ut->seninf = dev;

	return 0;
}

static void mtk_ut_seninf_component_unbind(struct device *dev,
					   struct device *master,
					   void *data)
{
	struct mtk_cam_ut *ut = data;

	dev_dbg(dev, "%s\n", __func__);
	ut->seninf = NULL;
}

static const struct component_ops mtk_ut_seninf_component_ops = {
	.bind = mtk_ut_seninf_component_bind,
	.unbind = mtk_ut_seninf_component_unbind,
};

static int mtk_ut_seninf_of_probe(struct platform_device *pdev,
			    struct mtk_ut_seninf_device *seninf)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
#ifndef FPGA_EP
	int i;
#endif

	/* base register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	seninf->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(seninf->base)) {
		dev_info(dev, "failed to map register base\n");
		return PTR_ERR(seninf->base);
	}
	dev_dbg(dev, "seninf, map_addr=0x%pK\n", seninf->base);

#ifndef FPGA_EP
	seninf->num_clks = of_count_phandle_with_args(pdev->dev.of_node,
						      "clocks", "#clock-cells");
	dev_info(dev, "clk_num:%d\n", seninf->num_clks);

	if (seninf->num_clks) {
		seninf->clks = devm_kcalloc(dev, seninf->num_clks,
					    sizeof(*seninf->clks), GFP_KERNEL);
		if (!seninf->clks)
			return -ENODEV;
	}

	for (i = 0; i < seninf->num_clks; i++) {
		seninf->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(seninf->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}
#endif

	return 0;
}

static int mtk_ut_seninf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_seninf_device *seninf;
	int ret;


#ifdef FPGA_EP
	pr_info("v4l2_subdev_mtk_ut_seninf_probe\n");
#endif
	dev_info(dev, "%s\n", __func__);

	seninf = devm_kzalloc(dev, sizeof(*seninf), GFP_KERNEL);
	if (!seninf)
		return -ENOMEM;

	seninf->dev = dev;
	dev_set_drvdata(dev, seninf);

	ret = mtk_ut_seninf_of_probe(pdev, seninf);
	if (ret)
		return ret;

	ut_seninf_set_ops(dev);

#ifndef FPGA_EP
	pm_runtime_enable(dev);
#endif

	ret = component_add(dev, &mtk_ut_seninf_component_ops);
	if (ret)
		return ret;

	dev_info(dev, "%s: success\n", __func__);
	return 0;
}

static int mtk_ut_seninf_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s\n", __func__);

	for (i = 0; i < seninf->num_clks; i++) {
		if (seninf->clks[i])
			clk_put(seninf->clks[i]);
	}

#ifndef FPGA_EP
	pm_runtime_disable(dev);
#endif

	component_del(dev, &mtk_ut_seninf_component_ops);
	return 0;
}

static int mtk_ut_seninf_pm_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_ut_seninf_pm_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_ut_seninf_runtime_suspend(struct device *dev)
{
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < seninf->num_clks; i++)
		clk_disable_unprepare(seninf->clks[i]);

	return 0;
}

static int mtk_ut_seninf_runtime_resume(struct device *dev)
{
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < seninf->num_clks; i++)
		clk_prepare_enable(seninf->clks[i]);

	return 0;
}

static const struct dev_pm_ops mtk_ut_seninf_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_ut_seninf_pm_suspend, mtk_ut_seninf_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_ut_seninf_runtime_suspend, mtk_ut_seninf_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_ut_seninf_of_ids[] = {
	{.compatible = "mediatek,seninf-core",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ut_seninf_of_ids);

struct platform_driver mtk_ut_seninf_driver = {
	.probe   = mtk_ut_seninf_probe,
	.remove  = mtk_ut_seninf_remove,
	.driver  = {
		.name  = "mtk-cam seninf-ut",
		.of_match_table = of_match_ptr(mtk_ut_seninf_of_ids),
		.pm     = &mtk_ut_seninf_pm_ops,
	}
};

