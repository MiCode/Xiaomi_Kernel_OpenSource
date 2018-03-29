/*
* Copyright (c) 2015 MediaTek Inc.
* Author: Tiffany Lin <tiffany.lin@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>

#include "mtk_vcodec_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcodec_iommu.h"
#include "mtk_vpu.h"

int mtk_vcodec_init_dec_pm(struct mtk_vcodec_dev *mtkdev)
{
	struct device_node *node;
	struct platform_device *pdev;
	struct device *dev;
	struct mtk_vcodec_pm *pm;
	int ret = 0;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	pm->mtkdev = mtkdev;
	dev = &pdev->dev;
	node = of_parse_phandle(dev->of_node, "larb", 0);
	if (!node)
		return -1;

	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -1;
	}
	pm->larbvdec = &pdev->dev;

	pdev = mtkdev->plat_dev;
	pm_runtime_enable(&pdev->dev);
	pm->dev = &pdev->dev;

	pm->vdecpll = devm_clk_get(&pdev->dev, "vdecpll");
	if (pm->vdecpll == NULL) {
		mtk_v4l2_err("devm_clk_get vdecpll fail");
		ret = -1;
	}

	pm->vdec_sel = devm_clk_get(&pdev->dev, "vdec_sel");
	if (pm->vdec_sel == NULL) {
		mtk_v4l2_err("devm_clk_get vdec_sel fail");
		ret = -1;
	}

	return ret;
}

void mtk_vcodec_release_dec_pm(struct mtk_vcodec_dev *dev)
{
	pm_runtime_disable(dev->pm.dev);
}

void mtk_vcodec_dec_pw_on(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_get_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_get_sync fail %d", ret);
}

void mtk_vcodec_dec_pw_off(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_put_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_put_sync fail %d", ret);
}

void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = clk_prepare_enable(pm->vdecpll);
	if (ret)
		mtk_v4l2_err("vdecpll fail %d", ret);

	ret = clk_prepare_enable(pm->vdec_sel);
	if (ret)
		mtk_v4l2_err("vdec_sel fail %d", ret);

	ret = clk_set_parent(pm->vdec_sel, pm->vdecpll);
	if (ret)
		mtk_v4l2_err("clk_set_parent fail %d", ret);

	ret = mtk_smi_larb_get(pm->larbvdec);
	if (ret)
		mtk_v4l2_err("mtk_smi_larb_get larbvdec fail %d", ret);

	mtk_vcodec_iommu_init(pm->dev);
}

void mtk_vcodec_dec_clock_off(struct mtk_vcodec_pm *pm)
{
	mtk_smi_larb_put(pm->larbvdec);
	clk_disable_unprepare(pm->vdec_sel);
	clk_disable_unprepare(pm->vdecpll);
}
