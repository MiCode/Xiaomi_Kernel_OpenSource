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

#if IS_ENABLED(CONFIG_MTK_PSEUDO_M4U)
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
#include "iommu_debug.h"
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
extern void vdec_dump_mem_buf(unsigned long h_vdec);
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
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", 0);
	if (!node) {
		mtk_v4l2_err("no mediatek,larbs found");
		return -1;
	}
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
		} else if (IS_SPECIFIC_CLK_TYPE(clk_name, MTK_VDEC_CLK_SOC_PREFIX)) {
			clks_data->soc_clks[clks_data->soc_clks_len].clk_id = clk_id;
			clks_data->soc_clks[clks_data->soc_clks_len].clk_name = clk_name;
			clks_data->soc_clks_len++;
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
	// dump soc clocks
	for (i = 0; i < clks_data->soc_clks_len; i++) {
		mtk_v4l2_debug(8, "core_clks id: %d, name: %s",
			clks_data->soc_clks[i].clk_id, clks_data->soc_clks[i].clk_name);
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

	ret = pm_runtime_resume_and_get(pm->dev);
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

static void mtk_vdec_hw_break(struct mtk_vcodec_dev *dev, int hw_id)
{
	u32 cg_status = 0, ufo_cg_status = 0;
	void __iomem *vdec_misc_addr = dev->dec_reg_base[VDEC_MISC];
	void __iomem *vdec_vld_addr = dev->dec_reg_base[VDEC_VLD];
	void __iomem *vdec_gcon_addr = dev->dec_reg_base[VDEC_SYS];
	void __iomem *vdec_ufo_addr = dev->dec_reg_base[VDEC_BASE] + 0x800;
	void __iomem *vdec_lat_misc_addr = dev->dec_reg_base[VDEC_LAT_MISC];
	void __iomem *vdec_lat_vld_addr = dev->dec_reg_base[VDEC_LAT_VLD];
	struct mtk_vcodec_ctx *ctx = NULL;
	int misc_offset[4] = {64, 66, 67, 65};

	struct timespec64 tv_start;
	struct timespec64 tv_end;
	s32 usec, timeout = 20000;
	int offset, idx;
	unsigned long value;
	u32 fourcc;
	u32 is_ufo = 0;

	if (hw_id == MTK_VDEC_CORE) {
		ctx = dev->curr_dec_ctx[hw_id];
		if (ctx)
			fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		else
			fourcc = v4l2_fourcc('U', 'N', 'K', 'N');

		if (vdec_vld_addr == NULL || vdec_misc_addr == NULL) {
			mtk_v4l2_debug(4, "VDEC codec:0x%08x(%c%c%c%c) HW break fail since vdec_vld_addr 0x%lx vdec_misc_addr 0x%lx",
				fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
				(fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF,
				(unsigned long)vdec_vld_addr, (unsigned long)vdec_misc_addr);
			return;
		}

		if (fourcc != V4L2_PIX_FMT_AV1 && dev->dec_reg_base[VDEC_BASE] != NULL)
			is_ufo = readl(vdec_ufo_addr + 0x08C) & 0x1;

		/* hw break */
		writel((readl(vdec_misc_addr + 0x0100) | 0x1), vdec_misc_addr + 0x0100);
		if (is_ufo)
			writel((readl(vdec_ufo_addr + 0x01C) & 0xFFFFFFFD), vdec_ufo_addr + 0x01C);

		mtk_vdec_do_gettimeofday(&tv_start);
		cg_status = readl(vdec_misc_addr + 0x0104);
		if (is_ufo)
			ufo_cg_status = readl(vdec_ufo_addr + 0x08C);
		while (((cg_status & 0x11) != 0x11) ||
			(is_ufo && ((ufo_cg_status & 0x11000) != 0x11000))) {
			mtk_vdec_do_gettimeofday(&tv_end);
			usec = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 +
				(tv_end.tv_nsec - tv_start.tv_nsec);
			if (usec > timeout) {
				mtk_v4l2_err("VDEC HW break timeout. codec:0x%08x(%c%c%c%c) ufo %d",
					fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
					(fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF, is_ufo);
				if (vdec_gcon_addr > 0) {
					value = readl(vdec_gcon_addr + (0 << 2));
					mtk_v4l2_err("[DEBUG][GCON] 0x%x(%d) = 0x%lx",
						0 << 2, 0, value);
					value = readl(vdec_gcon_addr + (6 << 2));
					mtk_v4l2_err("[DEBUG][GCON] 0x%x(%d) = 0x%lx",
						6 << 2, 6, value);
				}
				for (offset = 64; offset <= 79; offset++) {
					value = readl(vdec_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}
				for (idx = 0; idx < 4; idx++) {
					offset = misc_offset[idx];
					value = readl(vdec_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}
				if (is_ufo)
					mtk_v4l2_err("[DEBUG][UFO] 0x%x(%d) = 0x%x",
						0x08C, 0x08C >> 2, ufo_cg_status);

				if (timeout == 20000)
					timeout = 1000000;
				else if (timeout == 1000000) {
					/* v4l2_aee_print(
					 *    "%s %p codec:0x%08x(%c%c%c%c) hw break timeout\n",
					 *    __func__, ctx, fourcc,
					 *    fourcc & 0xFF, (fourcc >> 8) & 0xFF,
					 *    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF);
					 */
					break;
				}
				mtk_vdec_do_gettimeofday(&tv_start);
				//smi_debug_bus_hang_detect(0, "VCODEC");
			}
			cg_status = readl(vdec_misc_addr + 0x0104);
			if (is_ufo)
				ufo_cg_status = readl(vdec_ufo_addr + 0x08C);
		}

		/* sw reset */
		if (is_ufo)
			writel((readl(vdec_ufo_addr + 0x01C) | 0x2), vdec_ufo_addr + 0x01C);
		writel(0x1, vdec_vld_addr + 0x0108);
		writel(0x0, vdec_vld_addr + 0x0108);
	} else if (dev->pm.mtkdev->vdec_hw_ipm == VCODEC_IPM_V2 && hw_id == MTK_VDEC_LAT) {
		ctx = dev->curr_dec_ctx[hw_id];
		if (ctx)
			fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		else
			fourcc = v4l2_fourcc('U', 'N', 'K', 'N');

		if (vdec_lat_vld_addr == NULL || vdec_lat_misc_addr == NULL) {
			mtk_v4l2_debug(4, "VDEC codec:0x%08x(%c%c%c%c) HW break fail since vdec_lat_vld_addr 0x%lx vdec_lat_misc_addr 0x%lx",
				fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
				(fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF,
				(unsigned long)vdec_lat_vld_addr,
				(unsigned long)vdec_lat_misc_addr);
			return;
		}

		/* hw break */
		writel((readl(vdec_lat_misc_addr + 0x0100) | 0x1), vdec_lat_misc_addr + 0x0100);

		mtk_vdec_do_gettimeofday(&tv_start);
		cg_status = readl(vdec_lat_misc_addr + 0x0104);
		while (!((cg_status & 0x1) && (cg_status & 0x10))) {
			mtk_vdec_do_gettimeofday(&tv_end);
			usec = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 +
				(tv_end.tv_nsec - tv_start.tv_nsec);
			if (usec > timeout) {
				mtk_v4l2_err("VDEC HW %d break timeout. codec:0x%08x(%c%c%c%c)",
					hw_id, fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
					(fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF);
				for (offset = 64; offset <= 79; offset++) {
					value = readl(vdec_lat_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][LAT_MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}
				for (idx = 0; idx < 4; idx++) {
					offset = misc_offset[idx];
					value = readl(vdec_lat_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][LAT_MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}

				if (timeout == 20000)
					timeout = 1000000;
				else if (timeout == 1000000) {
					/* v4l2_aee_print(
					 *    "%s %p codec:0x%08x(%c%c%c%c) hw %d break timeout\n",
					 *    __func__, ctx, fourcc,
					 *    fourcc & 0xFF, (fourcc >> 8) & 0xFF,
					 *    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF, hw_id);
					 */
					break;
				}
				mtk_vdec_do_gettimeofday(&tv_start);
				//smi_debug_bus_hang_detect(0, "VCODEC");
			}
			cg_status = readl(vdec_lat_misc_addr + 0x0104);
		}

		/* sw reset */
		writel(0x1, vdec_lat_vld_addr + 0x0108);
		writel(0x0, vdec_lat_vld_addr + 0x0108);
	} else {
		mtk_v4l2_err("hw_id (%d) is unknown or unsupport\n", hw_id);
	}
}

void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm, int hw_id)
{

#if IS_ENABLED(CONFIG_MTK_PSEUDO_M4U)
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
	unsigned long flags;

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
		if (clk_id >= 0) {
			ret = clk_prepare_enable(pm->vdec_clks[clk_id]);
			if (ret)
				mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
					clk_id, clks_data->main_clks[j].clk_name, ret);
		}
	}

	if (hw_id == MTK_VDEC_CORE || hw_id == MTK_VDEC_LAT) {
		// enable soc clocks
		for (j = 0; j < clks_data->soc_clks_len; j++) {
			clk_id = clks_data->soc_clks[j].clk_id;
			if (clk_id >= 0) {
				ret = clk_prepare_enable(pm->vdec_clks[clk_id]);
				if (ret)
					mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
						clk_id, clks_data->soc_clks[j].clk_name, ret);
			}
		}
	}
	if (hw_id == MTK_VDEC_CORE) {
		// enable core clocks
		for (j = 0; j < clks_data->core_clks_len; j++) {
			clk_id = clks_data->core_clks[j].clk_id;
			if (clk_id >= 0) {
				ret = clk_prepare_enable(pm->vdec_clks[clk_id]);
				if (ret)
					mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
						clk_id, clks_data->core_clks[j].clk_name, ret);
			}
		}
	} else if (hw_id == MTK_VDEC_LAT) {
		// enable lat clocks
		for (j = 0; j < clks_data->lat_clks_len; j++) {
			clk_id = clks_data->lat_clks[j].clk_id;
			if (clk_id >= 0) {
				ret = clk_prepare_enable(pm->vdec_clks[clk_id]);
				if (ret)
					mtk_v4l2_err("clk_prepare_enable id: %d, name: %s fail %d",
						clk_id, clks_data->lat_clks[j].clk_name, ret);
			}
		}
	} else {
		mtk_v4l2_err("invalid hw_id %d", hw_id);
		time_check_end(MTK_FMT_DEC, hw_id, 50);
		return;
	}

	dev = container_of(pm, struct mtk_vcodec_dev, pm);
	if (!ret) {
		spin_lock_irqsave(&dev->dec_power_lock[hw_id], flags);
		dev->dec_is_power_on[hw_id] = true;
		spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);
	}

	if (pm->mtkdev->vdec_hw_ipm == VCODEC_IPM_V2) {
		mutex_lock(&pm->dec_racing_info_mutex);
		if (atomic_inc_return(&pm->dec_active_cnt) == 1) {
			/* restore racing info read/write ptr */
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

#if IS_ENABLED(CONFIG_MTK_PSEUDO_M4U)
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
	unsigned long flags;

	clks_data = &pm->vdec_clks_data;

	dev = container_of(pm, struct mtk_vcodec_dev, pm);
	if (pm->mtkdev->vdec_hw_ipm == VCODEC_IPM_V2) {
		mutex_lock(&pm->dec_racing_info_mutex);
		if (atomic_dec_and_test(&pm->dec_active_cnt)) {
			/* backup racing info read/write ptr */
			vdec_racing_addr =
				dev->dec_reg_base[VDEC_RACING_CTRL] +
					MTK_VDEC_RACING_INFO_OFFSET;
			for (i = 0; i < MTK_VDEC_RACING_INFO_SIZE; i++)
				pm->vdec_racing_info[i] =
					readl(vdec_racing_addr + i * 4);
		}
		mutex_unlock(&pm->dec_racing_info_mutex);
	}

	mtk_vdec_hw_break(dev, hw_id);

	/* avoid translation fault callback dump reg not done */
	spin_lock_irqsave(&dev->dec_power_lock[hw_id], flags);
	dev->dec_is_power_on[hw_id] = false;
	spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);

	if (hw_id == MTK_VDEC_CORE || hw_id == MTK_VDEC_LAT) {
		// disable soc clocks
		if (clks_data->soc_clks_len > 0) {
			for (i = clks_data->soc_clks_len - 1; i >= 0; i--) {
				clk_id = clks_data->soc_clks[i].clk_id;
				if (clk_id >= 0)
					clk_disable_unprepare(pm->vdec_clks[clk_id]);
			}
		}
	}
	if (hw_id == MTK_VDEC_CORE) {
		// disable core clocks
		if (clks_data->core_clks_len > 0) {
			for (i = clks_data->core_clks_len - 1; i >= 0; i--) {
				clk_id = clks_data->core_clks[i].clk_id;
				if (clk_id >= 0)
					clk_disable_unprepare(pm->vdec_clks[clk_id]);
			}
		}
	} else if (hw_id == MTK_VDEC_LAT) {
		// disable lat clocks
		if (clks_data->lat_clks_len > 0) {
			for (i = clks_data->lat_clks_len - 1; i >= 0; i--) {
				clk_id = clks_data->lat_clks[i].clk_id;
				if (clk_id >= 0)
					clk_disable_unprepare(pm->vdec_clks[clk_id]);
			}
		}
	} else
		mtk_v4l2_err("invalid hw_id %d", hw_id);

	if (clks_data->main_clks_len > 0) {
		for (i = clks_data->main_clks_len - 1; i >= 0; i--) {
			clk_id = clks_data->main_clks[i].clk_id;
			if (clk_id >= 0)
				clk_disable_unprepare(pm->vdec_clks[clk_id]);
		}
	}

	for (larb_index = 0; larb_index < MTK_VDEC_MAX_LARB_COUNT; larb_index++) {
		if (pm->larbvdecs[larb_index])
			mtk_smi_larb_put(pm->larbvdecs[larb_index]);
	}

#endif
}

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
static void mtk_vdec_dump_addr_reg(
	struct mtk_vcodec_dev *dev, int hw_id, enum mtk_dec_dump_addr_type type)
{
	struct mtk_vcodec_ctx *ctx;
	u32 fourcc;
	void __iomem *vld_addr = dev->dec_reg_base[VDEC_VLD];
	void __iomem *mc_addr = dev->dec_reg_base[VDEC_MC];
	void __iomem *mv_addr = dev->dec_reg_base[VDEC_MV];
	void __iomem *ufo_addr = dev->dec_reg_base[VDEC_BASE] + 0x800;
	void __iomem *lat_vld_addr = dev->dec_reg_base[VDEC_LAT_VLD];
	void __iomem *lat_wdma_addr = dev->dec_reg_base[VDEC_LAT_MISC] + 0x800;
	void __iomem *rctrl_addr = dev->dec_reg_base[VDEC_RACING_CTRL];
	void __iomem *misc_addr = dev->dec_reg_base[VDEC_MISC];
	enum mtk_vcodec_ipm vdec_hw_ipm;
	unsigned long value, values[6];
	bool is_ufo = false;
	int i, j, start, end;
	unsigned long flags;

	#define INPUT_LAT_VLD_NUM 7
	const unsigned int input_lat_vld_reg[INPUT_LAT_VLD_NUM] = {
		0xB0, 0xB4, 0xB8, 0x110, 0xEC, 0xF8, 0xFC};
	// RPTR, VSTART, VEND, WPTR, VBAR, VWPTR, VRPTR
	#define OUTPUT_MC_NUM 2
	const unsigned int output_mc_reg[OUTPUT_MC_NUM] = {
		0x224, 0x228}; // PY_ADD, PC_ADD
	#define OUTPUT_UFO_MC_NUM 5
	const unsigned int output_ufo_mc_reg[OUTPUT_UFO_MC_NUM] = {
		0xB5C, 0xAE8, 0xAEC, 0xCE4, 0xCE8};
	// YC_SEP, LEN_Y, LEN_C, LEN_Y_OFFSET, LEN_C_OFFSET
	#define OUTPUT_UFO_NUM 4
	const unsigned int output_ufo_reg[OUTPUT_UFO_NUM] = {
		0x7C, 0x80, 0x84, 0x88}; // LEN_Y, LEN_C, BS_Y, BS_C
	#define REF_MC_NUM 7
	const unsigned int ref_mc_base[REF_MC_NUM] = {
		0x3DC, 0xB60, 0x45C, 0xBE0, 0x4DC, 0xC60, 0xD28};
	// P_L0_Y, P_L0_C, B_L0_Y, B_L0_C, B_L1_Y, B_L1_C, REF
	#define UBE_CORE_VLD_NUM 3
	const unsigned int ube_core_vld_reg[UBE_CORE_VLD_NUM] = {
		0xB0, 0xB4, 0xB8};

	if (dev->pm.mtkdev == NULL) {
		mtk_v4l2_err("fail to get vdec_hw_ipm");
		vdec_hw_ipm = VCODEC_IPM_V2;
	} else {
		vdec_hw_ipm = dev->pm.mtkdev->vdec_hw_ipm;
	}
	if (hw_id != MTK_VDEC_CORE && (hw_id != MTK_VDEC_LAT || vdec_hw_ipm == VCODEC_IPM_V1)) {
		mtk_v4l2_err("hw_id %d not support !!", hw_id);
		return;
	}
	if (vdec_hw_ipm == VCODEC_IPM_V1)
		lat_vld_addr = vld_addr; // for ipm v1 input buffer

	ctx = dev->curr_dec_ctx[hw_id];
	if (ctx)
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	else
		fourcc = 0;

	spin_lock_irqsave(&dev->dec_power_lock[hw_id], flags);
	if (dev->dec_is_power_on[hw_id] == false) {
		mtk_v4l2_err("hw %d power is off !!", hw_id);
		spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);
		return;
	}

	if (hw_id == MTK_VDEC_CORE && fourcc != V4L2_PIX_FMT_AV1 && (ufo_addr - 0x800) != NULL)
		is_ufo = (readl(ufo_addr + 0x08C) & 0x1) == 0x1;

	switch (type) {
	case DUMP_VDEC_IN_BUF:
		if (lat_vld_addr == NULL)
			break;
		for (i = 0; i < INPUT_LAT_VLD_NUM; i++) {
			value = readl(lat_vld_addr + input_lat_vld_reg[i]);
			mtk_v4l2_err("[LAT][VLD] 0x%x(%d) = 0x%lx",
				input_lat_vld_reg[i], input_lat_vld_reg[i]/4, value);
		}
		if (fourcc == V4L2_PIX_FMT_VP8) {
			for (i = 41; i < 68; i++) {
				value = readl(misc_addr + 0x2800 + i*4);
				mtk_v4l2_err("[VP8_VLD] 0x%x(%d) = 0x%lx",
					i*4, i, value);
			}
			for (i = 72; i < 97; i++) {
				value = readl(misc_addr + 0x2800 + i*4);
				mtk_v4l2_err("[VP8_VLD] 0x%x(%d) = 0x%lx",
					i*4, i, value);
			}
			for (i = 66; i < 79; i++) {
				value = readl(misc_addr + i*4);
				mtk_v4l2_err("[MISC] 0x%x(%d) = 0x%lx",
					i*4, i, value);
			}
			value = readl(vld_addr + 0x800 + 15*4);
			mtk_v4l2_err("[VLD_TOP] 0x%x(%d) = 0x%lx",
				15*4, 15, value);
		}
		break;
	case DUMP_VDEC_OUT_BUF:
		if (mc_addr == NULL)
			break;
		for (i = 0; i < OUTPUT_MC_NUM; i++) {
			value = readl(mc_addr + output_mc_reg[i]);
			mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx",
				output_mc_reg[i], output_mc_reg[i]/4, value);
		}
		if (is_ufo) {
			for (i = 0; i < OUTPUT_UFO_MC_NUM; i++) {
				value = readl(mc_addr + output_ufo_mc_reg[i]);
				mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx",
					output_ufo_mc_reg[i], output_ufo_mc_reg[i]/4, value);
			}
			if ((ufo_addr - 0x800) == NULL)
				break;
			for (i = 0; i < OUTPUT_UFO_NUM; i++) {
				value = readl(ufo_addr + output_ufo_reg[i]);
				mtk_v4l2_err("[CORE][UFO] 0x%x(%d) = 0x%lx",
					output_ufo_reg[i], output_ufo_reg[i]/4, value);
			}
		}
		break;
	case DUMP_VDEC_REF_BUF:
		if (mc_addr == NULL)
			break;
		for (i = 0; i < 32; i++) {
			for (j = 0; j < 6; j++)
				values[j] = readl(mc_addr + (ref_mc_base[j] + i * 4));
			mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx",
				ref_mc_base[0] + i * 4, ref_mc_base[0]/4 + i, values[0],
				ref_mc_base[1] + i * 4, ref_mc_base[1]/4 + i, values[1],
				ref_mc_base[2] + i * 4, ref_mc_base[2]/4 + i, values[2],
				ref_mc_base[3] + i * 4, ref_mc_base[3]/4 + i, values[3],
				ref_mc_base[4] + i * 4, ref_mc_base[4]/4 + i, values[4],
				ref_mc_base[5] + i * 4, ref_mc_base[5]/4 + i, values[5]);
		}
		for (i = 0; i < 4; i++)
			values[i] = readl(mc_addr + i * 4);
		mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx",
			i * 4, i, values[0],
			i * 4, i, values[1],
			i * 4, i, values[2],
			i * 4, i, values[3]);
		for (i = 0; i < 6; i++)
			values[i] = readl(mc_addr + (ref_mc_base[6] + i * 4));
		mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx",
			ref_mc_base[6], ref_mc_base[6]/4 + 0, values[0],
			ref_mc_base[6] + 1 * 4, ref_mc_base[6]/4 + 1, values[1],
			ref_mc_base[6] + 2 * 4, ref_mc_base[6]/4 + 2, values[2],
			ref_mc_base[6] + 3 * 4, ref_mc_base[6]/4 + 3, values[3],
			ref_mc_base[6] + 4 * 4, ref_mc_base[6]/4 + 4, values[4],
			ref_mc_base[6] + 5 * 4, ref_mc_base[6]/4 + 5, values[5]);
		break;
	case DUMP_VDEC_MV_BUF:
		if (hw_id != MTK_VDEC_CORE) {
			mtk_v4l2_err("not support dump MV at hw_id %d", hw_id);
			break;
		}
		if (mv_addr == NULL)
			break;
		value = readl(mv_addr + 0x20C);
		mtk_v4l2_err("[CORE][MV] 0x%x(%d) = 0x%lx",
			0x20C, 0x20C/4, value);
		switch (fourcc) {
		case V4L2_PIX_FMT_H265:
			start = 0;
			end = 32;
			break;
		case V4L2_PIX_FMT_H264:
			start = 96;
			end = 128;
			break;
		case V4L2_PIX_FMT_VP9:
			start = 240;
			end = 241;
			break;
		case V4L2_PIX_FMT_AV1:
			start = 353;
			end = 356;
			break;
		default:
			start = 195;
			end = 198;
		}
		for (i = start; i < end; i++) {
			value = readl(mv_addr + i * 4);
			mtk_v4l2_err("[CORE][MV] 0x%x(%d) = 0x%lx", i * 4, i, value);
		}
		break;
	case DUMP_VDEC_UBE_BUF:
		if (hw_id == MTK_VDEC_LAT) {
			if ((lat_wdma_addr - 0x800) != NULL) {
				value = readl(lat_wdma_addr + 0x50);
				mtk_v4l2_err("[LAT][WDMA] 0x%x(%d) = 0x%lx",
					0x50, 0x50/4, value);
				value = readl(lat_wdma_addr + 0x44);
				mtk_v4l2_err("[LAT][WDMA] 0x%x(%d) = 0x%lx",
					0x44, 0x44/4, value);
			}
			if (rctrl_addr != NULL) {
				value = readl(rctrl_addr + 0x78);
				mtk_v4l2_err("[RACING_CTRL] 0x%x(%d) = 0x%lx",
					0x78, 0x78/4, value);
			}
		} else {
			if (rctrl_addr != NULL) {
				value = readl(rctrl_addr + 0x7C);
				mtk_v4l2_err("[RACING_CTRL] 0x%x(%d) = 0x%lx",
					0x7C, 0x7C/4, value);
			}
			if (vld_addr != NULL) {
				for (i = 0; i < UBE_CORE_VLD_NUM; i++) {
					value = readl(vld_addr + ube_core_vld_reg[i]);
					mtk_v4l2_err("[CORE][VLD] 0x%x(%d) = 0x%lx",
					    ube_core_vld_reg[i], ube_core_vld_reg[i]/4, value);
				}
			}
		}
		break;
	default:
		mtk_v4l2_err("unknown addr type");
	}

	spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);
}

static int mtk_vdec_translation_fault_callback(
	int port, dma_addr_t mva, void *data)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)data;
	int hw_id;
	struct mtk_vcodec_ctx *ctx;
	u32 fourcc;
	enum mtk_vcodec_ipm vdec_hw_ipm;
	int port_idx;

	if (dev->pm.mtkdev == NULL) {
		mtk_v4l2_err("fail to get vdec_hw_ipm");
		vdec_hw_ipm = VCODEC_IPM_V2;
	} else {
		vdec_hw_ipm = dev->pm.mtkdev->vdec_hw_ipm;
	}

	for (port_idx = 0; port_idx < NUM_MAX_VDEC_M4U_PORT; port_idx++)
		if (port == dev->dec_m4u_ports[port_idx])
			break;

	if (port_idx == VDEC_M4U_PORT_LAT0_UFO ||
	    port_idx == VDEC_M4U_PORT_LAT0_UFO_C ||
	    port_idx == VDEC_M4U_PORT_LAT0_MC)
		hw_id = MTK_VDEC_CORE;
	else if (MTK_M4U_TO_LARB(port) == 4)
		hw_id = MTK_VDEC_CORE; // larb4 CORE
	else if (MTK_M4U_TO_LARB(port) == 5 && vdec_hw_ipm == VCODEC_IPM_V2)
		hw_id = MTK_VDEC_LAT; // larb5 LAT
	else {
		mtk_v4l2_err("unknown larb port %d of m4u port 0x%x", MTK_M4U_TO_LARB(port), port);
		return 0;
	}

	ctx = dev->curr_dec_ctx[hw_id];
	if (ctx) {
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		mtk_v4l2_err("[%d] codec:0x%08x(%c%c%c%c) %s(%d) TF larb %d port %s(%x) mva 0x%llx",
			ctx->id, fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
			(fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF,
			(hw_id == MTK_VDEC_LAT) ? "LAT" : "CORE", hw_id,
			MTK_M4U_TO_LARB(port), dec_port_name[port_idx], port, (u64)mva);
	} else {
		mtk_v4l2_err("ctx NULL codec unknown, %s(%d) TF larb %d port %s(%x) mva 0x%llx",
			(hw_id == MTK_VDEC_LAT) ? "LAT" : "CORE", hw_id,
			MTK_M4U_TO_LARB(port), dec_port_name[port_idx], port, (u64)mva);
	}

	if (port == dev->dec_m4u_ports[VDEC_M4U_PORT_LAT0_VLD] ||
	    port == dev->dec_m4u_ports[VDEC_M4U_PORT_LAT0_VLD2]) {
		mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_IN_BUF);
	} else if (port == dev->dec_m4u_ports[VDEC_M4U_PORT_PP] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_UFO] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_UFO_ENC] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_LAT0_UFO] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_LAT0_UFO_C]) {
		mtk_vdec_dump_addr_reg(dev, MTK_VDEC_CORE, DUMP_VDEC_OUT_BUF);
		if (port == dev->dec_m4u_ports[VDEC_M4U_PORT_UFO] ||
		    port == dev->dec_m4u_ports[VDEC_M4U_PORT_UFO_ENC])
			mtk_vdec_dump_addr_reg(dev, MTK_VDEC_CORE, DUMP_VDEC_REF_BUF);
	} else if (port == dev->dec_m4u_ports[VDEC_M4U_PORT_MC] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_LAT0_MC]) {
		mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_REF_BUF);
	} else if (port == dev->dec_m4u_ports[VDEC_M4U_PORT_AVC_MV]) {
		mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_MV_BUF);
	} else if (port == dev->dec_m4u_ports[VDEC_M4U_PORT_VLD] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_VLD2] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_RG_CTRL_DMA] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_LAT0_RG_CTRL_DMA] ||
		   port == dev->dec_m4u_ports[VDEC_M4U_PORT_LAT0_WDMA]) {
		if (vdec_hw_ipm == VCODEC_IPM_V2)
			mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_UBE_BUF);
		else
			mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_IN_BUF);
	} else {
		if (vdec_hw_ipm == VCODEC_IPM_V2) {
			if (hw_id == MTK_VDEC_CORE) {
				mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_UBE_BUF);
				mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_OUT_BUF);
			} else {
				mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_IN_BUF);
				mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_UBE_BUF);
			}
		} else {
			mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_IN_BUF);
			mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_OUT_BUF);
		}
	}

	return 0;
}

static int mtk_vdec_uP_translation_fault_callback(
	int port, dma_addr_t mva, void *data)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)data;
	struct mtk_vcodec_ctx *ctx, *dec_ctx[MTK_VDEC_HW_NUM];
	u32 dec_fourcc[MTK_VDEC_HW_NUM];
	char dec_codec_name[MTK_VDEC_HW_NUM][5];
	int dec_ctx_id[MTK_VDEC_HW_NUM];
	enum mtk_vcodec_ipm vdec_hw_ipm;
	int hw_id, i;
	struct list_head *list_ptr, *tmp;

	if (dev->pm.mtkdev == NULL) {
		mtk_v4l2_err("fail to get vdec_hw_ipm");
		vdec_hw_ipm = VCODEC_IPM_V2;
	} else {
		vdec_hw_ipm = dev->pm.mtkdev->vdec_hw_ipm;
	}

	for (hw_id = 0; hw_id < MTK_VDEC_HW_NUM; hw_id++) {
		dec_ctx[hw_id] = dev->curr_dec_ctx[hw_id];
		if (dec_ctx[hw_id]) {
			dec_ctx_id[hw_id] = dec_ctx[hw_id]->id;
			dec_fourcc[hw_id] = dec_ctx[hw_id]->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
			for (i = 0; i < 4; i++)
				dec_codec_name[hw_id][i] = (dec_fourcc[hw_id] >> (i * 8)) & 0xFF;
		} else {
			dec_ctx_id[hw_id] = 0;
			dec_fourcc[hw_id] = 0;
			if (sprintf(dec_codec_name[hw_id], "NULL") < 0)
				mtk_v4l2_err("dec_codec_name fail");
		}
		dec_codec_name[hw_id][4] = '\0';
	}

	mtk_v4l2_err("larb %d port VIDEO_uP(%x) translation fault, mva 0x%llx",
		MTK_M4U_TO_LARB(port), port, (u64)mva);
	mtk_v4l2_err("current dec ctx: LAT ctx_id %d codec:%s(0x%08x), CORE ctx_id %d codec:%s(0x%08x) (ipm v%d)",
		dec_ctx_id[MTK_VDEC_LAT], dec_codec_name[MTK_VDEC_LAT], dec_fourcc[MTK_VDEC_LAT],
		dec_ctx_id[MTK_VDEC_CORE], dec_codec_name[MTK_VDEC_CORE],
		dec_fourcc[MTK_VDEC_CORE], vdec_hw_ipm);

	mtk_v4l2_err("dec working buffer:");
	mutex_lock(&dev->ctx_mutex);
	list_for_each_safe(list_ptr, tmp, &dev->ctx_list) {
		ctx = list_entry(list_ptr, struct mtk_vcodec_ctx, list);
		if (ctx != NULL && ctx->state != MTK_STATE_ABORT)
			vdec_dump_mem_buf(ctx->drv_handle);
	}
	mutex_unlock(&dev->ctx_mutex);
#endif

	return 0;
}
#endif

int mtk_vdec_m4u_port_name_to_index(const char *name)
{
	if (!strcmp(MTK_VDEC_M4U_PORT_NAME_MC, name))
		return VDEC_M4U_PORT_MC;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_UFO, name))
		return VDEC_M4U_PORT_UFO;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_PP, name))
		return VDEC_M4U_PORT_PP;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_PRED_RD, name))
		return VDEC_M4U_PORT_PRED_RD;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_PRED_WR, name))
		return VDEC_M4U_PORT_PRED_WR;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_PPWRAP, name))
		return VDEC_M4U_PORT_PPWRAP;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_TILE, name))
		return VDEC_M4U_PORT_TILE;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_VLD, name))
		return VDEC_M4U_PORT_VLD;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_VLD2, name))
		return VDEC_M4U_PORT_VLD2;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_AVC_MV, name))
		return VDEC_M4U_PORT_AVC_MV;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_RG_CTRL_DMA, name))
		return VDEC_M4U_PORT_RG_CTRL_DMA;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_UFO_ENC, name))
		return VDEC_M4U_PORT_UFO_ENC;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_VLD, name))
		return VDEC_M4U_PORT_LAT0_VLD;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_VLD2, name))
		return VDEC_M4U_PORT_LAT0_VLD2;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_AVC_MV, name))
		return VDEC_M4U_PORT_LAT0_AVC_MV;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_PRED_RD, name))
		return VDEC_M4U_PORT_LAT0_PRED_RD;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_TILE, name))
		return VDEC_M4U_PORT_LAT0_TILE;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_WDMA, name))
		return VDEC_M4U_PORT_LAT0_WDMA;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_RG_CTRL_DMA, name))
		return VDEC_M4U_PORT_LAT0_RG_CTRL_DMA;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_MC, name))
		return VDEC_M4U_PORT_LAT0_MC;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_UFO_ENC, name))
		return VDEC_M4U_PORT_LAT0_UFO;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_LAT0_UFO_ENC_C, name))
		return VDEC_M4U_PORT_LAT0_UFO_C;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_UP_1, name))
		return VDEC_M4U_PORT_UP_1;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_UP_2, name))
		return VDEC_M4U_PORT_UP_2;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_UP_3, name))
		return VDEC_M4U_PORT_UP_3;
	else if (!strcmp(MTK_VDEC_M4U_PORT_NAME_UP_4, name))
		return VDEC_M4U_PORT_UP_4;
	else
		return -1;
}

void mtk_vdec_translation_fault_callback_setting(
	struct mtk_vcodec_dev *dev)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
	int i;

	for (i = 0; i < NUM_MAX_VDEC_M4U_PORT; i++) {
		if (dev->dec_m4u_ports[i] != 0 && i < VDEC_M4U_PORT_UP_1)
			mtk_iommu_register_fault_callback(dev->dec_m4u_ports[i],
				mtk_vdec_translation_fault_callback, (void *)dev, false);
		if (dev->dec_m4u_ports[i] != 0 && i >= VDEC_M4U_PORT_UP_1)
			mtk_iommu_register_fault_callback(dev->dec_m4u_ports[i],
				mtk_vdec_uP_translation_fault_callback, (void *)dev, false);
	}
#endif
}

