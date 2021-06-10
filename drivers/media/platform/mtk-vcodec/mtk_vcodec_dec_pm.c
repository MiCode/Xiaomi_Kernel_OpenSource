// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>
#include <linux/slab.h>
//#include "smi_public.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

void mtk_dec_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
	ctx->input_driven = 0;
	ctx->user_lock_hw = 1;
}

int mtk_vcodec_init_dec_pm(struct mtk_vcodec_dev *mtkdev)
{
	int ret = 0;
#ifndef FPGA_PWRCLK_API_DISABLE
	struct device_node *node;
	struct platform_device *pdev;
	struct mtk_vcodec_pm *pm;
	int larb_index;
	int i = 0;
	int clk_id = 0;
	const char *clk_name;
	struct mtk_vdec_clks_data *clks_data;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	pm->dev = &pdev->dev;
	pm->mtkdev = mtkdev;
	clks_data = &pm->vdec_clks_data;

	if (!pdev->dev.of_node) {
		mtk_v4l2_err("[VCODEC][ERROR] DTS went wrong...");
		return -ENODEV;
	}

	// parse "mediatek,larbs"
	for (larb_index = 0; larb_index < MTK_VDEC_MAX_LARB_COUNT; larb_index++) {
		node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", larb_index);
		if (!node)
			break;

		pdev = of_find_device_by_node(node);
		if (WARN_ON(!pdev)) {
			of_node_put(node);
			return -1;
		}
		pm->larbvdecs[larb_index] = &pdev->dev;
		mtk_v4l2_debug(2, "larbvdecs[%d] = %p", larb_index, pm->larbvdecs[larb_index]);
		pdev = mtkdev->plat_dev;
	}

	memset(clks_data, 0x00, sizeof(struct mtk_vdec_clks_data));
	while (!of_property_read_string_index(
			pdev->dev.of_node, "clock-names", clk_id, &clk_name)) {
		mtk_v4l2_debug(2, "init clock, id: %d, name: %s", clk_id, clk_name);
		pm->vdec_clks[clk_id] = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(pm->vdec_clks[clk_id])) {
			mtk_v4l2_err(
				"[VCODEC][ERROR] Unable to devm_clk_get id: %d, name: %s\n",
				clk_id, clk_name);
			return PTR_ERR(pm->vdec_clks[clk_id]);
		}
		if (IS_SPECIFIC_CLK_TYPE(clk_name, MTK_VDEC_CLK_MAIN_PREFIX)) {
			clks_data->main_clks[clks_data->main_clks_len].clk_id = clk_id;
			clks_data->main_clks[clks_data->main_clks_len].clk_name = clk_name;
			clks_data->main_clks_len++;
		} else if (IS_SPECIFIC_CLK_TYPE(clk_name, MTK_VDEC_CLK_CORE_PREFIX)) {
			clks_data->core_clks[clks_data->core_clks_len].clk_id = clk_id;
			clks_data->core_clks[clks_data->core_clks_len].clk_name = clk_name;
			clks_data->core_clks_len++;
		} else if (IS_SPECIFIC_CLK_TYPE(clk_name, MTK_VDEC_CLK_LAT_PREFIX)) {
			clks_data->lat_clks[clks_data->lat_clks_len].clk_id = clk_id;
			clks_data->lat_clks[clks_data->lat_clks_len].clk_name = clk_name;
			clks_data->lat_clks_len++;
		}
		clk_id++;
	}

#if DEBUG_GKI
	// dump main clocks
	for (i = 0; i < clks_data->main_clks_len; i++) {
		mtk_v4l2_debug(8, "main_clks id: %d, name: %s",
			clks_data->main_clks[i].clk_id, clks_data->main_clks[i].clk_name);
	}
	// dump core clocks
	for (i = 0; i < clks_data->core_clks_len; i++) {
		mtk_v4l2_debug(8, "core_clks id: %d, name: %s",
			clks_data->core_clks[i].clk_id, clks_data->core_clks[i].clk_name);
	}
	// dump lat clocks
	for (i = 0; i < clks_data->lat_clks_len; i++) {
		mtk_v4l2_debug(8, "lat_clks id: %d, name: %s",
			clks_data->lat_clks[i].clk_id, clks_data->lat_clks[i].clk_name);
	}
#endif

	if (pm->mtkdev->vdec_hw_ipm == VCODEC_IPM_V2) {
		atomic_set(&pm->dec_active_cnt, 0);
		memset(pm->vdec_racing_info, 0, sizeof(pm->vdec_racing_info));
		mutex_init(&pm->dec_racing_info_mutex);
	}

	pm_runtime_enable(&pdev->dev);
#endif

	return ret;
}

void mtk_vcodec_release_dec_pm(struct mtk_vcodec_dev *dev)
{
#if DEC_DVFS
	mutex_lock(&dev->dec_dvfs_mutex);
	mutex_unlock(&dev->dec_dvfs_mutex);
#endif
	pm_runtime_disable(dev->pm.dev);
}

void mtk_vcodec_dec_pw_on(struct mtk_vcodec_pm *pm, int hw_id)
{
	int ret;

	ret = pm_runtime_get_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_get_sync fail");
}

void mtk_vcodec_dec_pw_off(struct mtk_vcodec_pm *pm, int hw_id)
{
	int ret;

	ret = pm_runtime_put_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_put_sync fail");
}

void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm, int hw_id)
{

#ifdef CONFIG_MTK_PSEUDO_M4U
	int i, larb_port_num, larb_id;
	struct M4U_PORT_STRUCT port;
#endif

#ifndef FPGA_PWRCLK_API_DISABLE
	int j, ret;
	struct mtk_vcodec_dev *dev;
	void __iomem *vdec_racing_addr;
	int larb_index;
	int clk_id;
	struct mtk_vdec_clks_data *clks_data;

	time_check_start(MTK_FMT_DEC, hw_id);

	clks_data = &pm->vdec_clks_data;

	for (larb_index = 0; larb_index < MTK_VDEC_MAX_LARB_COUNT; larb_index++) {
		if (pm->larbvdecs[larb_index]) {
			ret = mtk_smi_larb_get(pm->larbvdecs[larb_index]);
			if (ret)
				mtk_v4l2_err("Failed to get vdec larb. index: %d, hw_id: %d",
					larb_index, hw_id);
		}
	}

	// enable main clocks
	for (j = 0; j < clks_data->main_clks_len; j++) {
		clk_id = clks_data->main_clks[j].clk_id;
		ret = clk_prepare_enable(pm->vdec_clks[clk_id]);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
				clk_id, clks_data->main_clks[j].clk_name, ret);
	}

	if (hw_id == MTK_VDEC_CORE) {
		// enable core clocks
		for (j = 0; j < clks_data->core_clks_len; j++) {
			clk_id = clks_data->core_clks[j].clk_id;
			ret = clk_prepare_enable(pm->vdec_clks[clk_id]);
			if (ret)
				mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
					clk_id, clks_data->core_clks[j].clk_name, ret);
		}
	} else if (hw_id == MTK_VDEC_LAT) {
		// enable lat clocks
		for (j = 0; j < clks_data->lat_clks_len; j++) {
			clk_id = clks_data->lat_clks[j].clk_id;
			ret = clk_prepare_enable(pm->vdec_clks[clk_id]);
			if (ret)
				mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
					clk_id, clks_data->lat_clks[j].clk_name, ret);
		}
	} else {
		mtk_v4l2_err("invalid hw_id %d", hw_id);
		time_check_end(MTK_FMT_DEC, hw_id, 50);
		return;
	}

	if (pm->mtkdev->vdec_hw_ipm == VCODEC_IPM_V2) {
		mutex_lock(&pm->dec_racing_info_mutex);
		if (atomic_inc_return(&pm->dec_active_cnt) == 1) {
			/* restore racing info read/write ptr */
			dev = container_of(pm, struct mtk_vcodec_dev, pm);
			vdec_racing_addr =
				dev->dec_reg_base[VDEC_RACING_CTRL] +
					MTK_VDEC_RACING_INFO_OFFSET;
			for (j = 0; j < MTK_VDEC_RACING_INFO_SIZE; j++)
				writel(pm->vdec_racing_info[j],
					vdec_racing_addr + j * 4);
		}
		mutex_unlock(&pm->dec_racing_info_mutex);
	}

	time_check_end(MTK_FMT_DEC, hw_id, 50);
#endif

#ifdef CONFIG_MTK_PSEUDO_M4U
	time_check_start(MTK_FMT_DEC, hw_id);
	if (hw_id == MTK_VDEC_CORE) {
		larb_port_num = SMI_LARB4_PORT_NUM;
		larb_id = 4;

		//enable UFO port
		port.ePortID = M4U_PORT_L5_VDEC_UFO_ENC_EXT;
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
	} else if (hw_id == MTK_VDEC_LAT) {
		larb_port_num = SMI_LARB5_PORT_NUM;
		larb_id = 5;
	}

	//enable 34bits port configs & sram settings
	for (i = 0; i < larb_port_num; i++) {
		port.ePortID = MTK_M4U_ID(larb_id, i);
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
	}
	time_check_end(MTK_FMT_DEC, hw_id, 50);
#endif

}

void mtk_vcodec_dec_clock_off(struct mtk_vcodec_pm *pm, int hw_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	struct mtk_vcodec_dev *dev;
	void __iomem *vdec_racing_addr;
	int i;
	int larb_index;
	int clk_id;
	struct mtk_vdec_clks_data *clks_data;

	clks_data = &pm->vdec_clks_data;

	if (pm->mtkdev->vdec_hw_ipm == VCODEC_IPM_V2) {
		mutex_lock(&pm->dec_racing_info_mutex);
		if (atomic_dec_and_test(&pm->dec_active_cnt)) {
			/* backup racing info read/write ptr */
			dev = container_of(pm, struct mtk_vcodec_dev, pm);
			vdec_racing_addr =
				dev->dec_reg_base[VDEC_RACING_CTRL] +
					MTK_VDEC_RACING_INFO_OFFSET;
			for (i = 0; i < MTK_VDEC_RACING_INFO_SIZE; i++)
				pm->vdec_racing_info[i] =
					readl(vdec_racing_addr + i * 4);
		}
		mutex_unlock(&pm->dec_racing_info_mutex);
	}

	if (hw_id == MTK_VDEC_CORE) {
		// disable core clocks
		if (clks_data->core_clks_len > 0) {
			for (i = clks_data->core_clks_len - 1; i >= 0; i--) {
				clk_id = clks_data->core_clks[i].clk_id;
				clk_disable_unprepare(pm->vdec_clks[clk_id]);
			}
		}
	} else if (hw_id == MTK_VDEC_LAT) {
		// disable lat clocks
		if (clks_data->lat_clks_len > 0) {
			for (i = clks_data->lat_clks_len - 1; i >= 0; i--) {
				clk_id = clks_data->lat_clks[i].clk_id;
				clk_disable_unprepare(pm->vdec_clks[clk_id]);
			}
		}
	} else
		mtk_v4l2_err("invalid hw_id %d", hw_id);

	if (clks_data->main_clks_len > 0) {
		for (i = clks_data->main_clks_len - 1; i >= 0; i--) {
			clk_id = clks_data->main_clks[i].clk_id;
			clk_disable_unprepare(pm->vdec_clks[clk_id]);
		}
	}

	for (larb_index = 0; larb_index < MTK_VDEC_MAX_LARB_COUNT; larb_index++) {
		if (pm->larbvdecs[larb_index])
			mtk_smi_larb_put(pm->larbvdecs[larb_index]);
	}

#endif
}

