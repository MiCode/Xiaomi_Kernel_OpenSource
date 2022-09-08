// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *       Tiffany Lin <tiffany.lin@mediatek.com>
 */
#include <mtk_vcodec_pm_codec.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <soc/mediatek/smi.h>
int mtk_vcodec_init_pm(struct mtk_vcodec_dev *mtkdev)
{
	struct platform_device *pdev;
	struct device *dev;
	struct mtk_vcodec_pm *pm;
	int clk_id = 0;
	struct mtk_clks_data *clks_data;
	int ret = 0;
	int larb_index;
	const char *clk_name;
	struct device_node *node;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	memset(pm, 0, sizeof(struct mtk_vcodec_pm));
	pm->mtkdev = mtkdev;
	pm->dev = &pdev->dev;
	clks_data = &pm->clks_data;
	dev = &pdev->dev;

	node = of_parse_phandle(dev->of_node, "mediatek,larbs", 0);
	if (!node) {
		pr_info("no mediatek,larbs found");
		return -1;
	}
	for (larb_index = 0; larb_index < MTK_MAX_LARB_COUNT; larb_index++) {
		node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", larb_index);
		if (!node)
			break;
		pdev = of_find_device_by_node(node);
		if (WARN_ON(!pdev)) {
			of_node_put(node);
			return -1;
		}
		pm->larbvencs[larb_index] = &pdev->dev;
		pm->larbvdecs[larb_index] = &pdev->dev;
		pr_info("larbvencs[%d] = %p", larb_index, pm->larbvencs[larb_index]);
		pr_info("larbvdecs[%d] = %p", larb_index, pm->larbvencs[larb_index]);
		pdev = mtkdev->plat_dev;
	}

	memset(clks_data, 0x00, sizeof(struct mtk_clks_data));
	while (!of_property_read_string_index(
			pdev->dev.of_node, "clock-names", clk_id, &clk_name)) {
		pm->vcodec_clks[clk_id] = devm_clk_get(&pdev->dev, clk_name);
		pr_info("%s init clock, id: %d, name: %s",  __func__, clk_id, clk_name);
		if (IS_ERR(pm->vcodec_clks[clk_id])) {
			pr_info(
				"[VCODEC][ERROR] Unable to devm_clk_get id: %d, name: %s\n",
				clk_id, clk_name);
			return PTR_ERR(pm->vcodec_clks[clk_id]);
		}
		clks_data->core_clks[clks_data->core_clks_len].clk_id = clk_id;
		clks_data->core_clks[clks_data->core_clks_len].clk_name = clk_name;
		clks_data->core_clks_len++;
		clk_id++;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_init_pm);
void mtk_vcodec_clock_on(enum mtk_instance_type type, struct mtk_vcodec_dev *mtkDev)
{
	int j, clk_id;
	int ret = 0;
	struct mtk_vcodec_pm *pm;
	struct mtk_clks_data *clks_data;

	pm = &mtkDev->pm;
	clks_data = &pm->clks_data;
	if (type == MTK_INST_ENCODER) {
		if (pm->larbvencs[0]) {
			ret = mtk_smi_larb_get(pm->larbvencs[0]);
			if (ret)
				pr_info("Failed to get venc larb");
		}
	}
	if (type == MTK_INST_DECODER) {
		if (pm->larbvdecs[0]) {
			ret = mtk_smi_larb_get(pm->larbvdecs[0]);
			if (ret)
				pr_info("Failed to get vdec larb");
		}
	}
	for (j = 0; j < clks_data->core_clks_len; j++) {
		clk_id = clks_data->core_clks[j].clk_id;
		if (type == MTK_INST_DECODER &&
			(!strcmp("MT_CG_VENC", clks_data->core_clks[j].clk_name))) {
			continue;
		} else if (type == MTK_INST_ENCODER &&
			(!strcmp("MT_CG_VDEC", clks_data->core_clks[j].clk_name))) {
			continue;
		} else {
			ret = clk_prepare_enable(pm->vcodec_clks[clk_id]);
			if (ret) {
				pr_info("clk_prepare_enable id: %d, name: %s fail %d",
					clk_id, clks_data->core_clks[j].clk_name, ret);
			}
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_clock_on);
void mtk_vcodec_clock_off(enum mtk_instance_type type, struct mtk_vcodec_dev *mtkDev)
{
	int i, clk_id;
	struct mtk_vcodec_pm *pm;
	struct mtk_clks_data *clks_data;

	pm = &mtkDev->pm;
	clks_data = &pm->clks_data;
		if (clks_data->core_clks_len > 0) {
			for (i = clks_data->core_clks_len - 1; i >= 0; i--) {
				if (type == MTK_INST_DECODER &&
					(!strcmp("MT_CG_VENC", clks_data->core_clks[i].clk_name))) {
					continue;
				} else if (type == MTK_INST_ENCODER &&
					(!strcmp("MT_CG_VDEC", clks_data->core_clks[i].clk_name))) {
					continue;
				} else {
					clk_id = clks_data->core_clks[i].clk_id;
					clk_disable_unprepare(pm->vcodec_clks[clk_id]);
				}
			}
		}
	if (type == MTK_INST_ENCODER) {
		if (pm->larbvencs[0])
			mtk_smi_larb_put(pm->larbvencs[0]);
	}
	if (type == MTK_INST_DECODER) {
		if (pm->larbvdecs[0])
			mtk_smi_larb_put(pm->larbvdecs[0]);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_clock_off);

MODULE_LICENSE("GPL v2");
