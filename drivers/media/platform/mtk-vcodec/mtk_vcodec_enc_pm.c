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


int mtk_vcodec_init_enc_pm(struct mtk_vcodec_dev *mtkdev)
{
	struct device_node *node;
	struct platform_device *pdev;
	struct device *dev;
	struct mtk_vcodec_pm *pm;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	memset(pm, 0, sizeof(struct mtk_vcodec_pm));
	pm->mtkdev = mtkdev;
	dev = &pdev->dev;

	node = of_parse_phandle(dev->of_node, "larb", 0);
	if (!node)
		return -ENODEV;

	pdev = of_find_device_by_node(node);
	of_node_put(node);
	if (WARN_ON(!pdev))
		return -ENODEV;

	pm->larbvenc = &pdev->dev;
	pdev = mtkdev->plat_dev;
	pm_runtime_enable(&pdev->dev);
	pm->dev = &pdev->dev;

	pm->venc = devm_clk_get(&pdev->dev, "venc");
	if (IS_ERR(pm->venc)) {
		pm_runtime_disable(&pdev->dev);
		mtk_v4l2_err("devm_clk_get venc fail");
		return PTR_ERR(pm->venc);
	}

	pm->venclt = devm_clk_get(&pdev->dev, "venclt");
	if (IS_ERR(pm->venclt)) {
		pm_runtime_disable(&pdev->dev);
		mtk_v4l2_err("devm_clk_get venclt fail");
		return PTR_ERR(pm->venclt);
	}

	return 0;
}

void mtk_vcodec_release_enc_pm(struct mtk_vcodec_dev *dev)
{
	pm_runtime_disable(dev->pm.dev);
}

void mtk_vcodec_enc_pw_on(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_get_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_get_sync fail %s\n",
				dev_name(pm->dev));

}

void mtk_vcodec_enc_pw_off(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_put_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_put_sync fail %s\n",
				dev_name(pm->dev));
}

void mtk_vcodec_enc_clock_on(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = mtk_smi_larb_get(pm->larbvenc);
	if (ret)
		mtk_v4l2_err("mtk_smi_larb_get larb3 fail %d\n", ret);

	mtk_vcodec_iommu_init(pm->dev);

	ret = clk_prepare_enable(pm->venc);
	if (ret)
		mtk_v4l2_err("venc fail %d", ret);

	ret = clk_prepare_enable(pm->venclt);
	if (ret)
		mtk_v4l2_err("vdec_sel venc_lt %d", ret);

}

void mtk_vcodec_enc_clock_off(struct mtk_vcodec_pm *pm)
{
	mtk_smi_larb_put(pm->larbvenc);
	clk_disable_unprepare(pm->venc);
	clk_disable_unprepare(pm->venclt);
}
