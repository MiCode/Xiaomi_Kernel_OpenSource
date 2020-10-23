// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>
#include <linux/slab.h>
//#include "smi_public.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

#if DEC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#define STD_VDEC_FREQ 218
#endif

#define VDEC_DRV_UFO_AUO_ON (1 << 1)
#if DEC_EMI_BW
#include <linux/interconnect-provider.h>
/* LARB4, mostly core */
/* LARB5, mostly lat */
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

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	pm->mtkdev = mtkdev;
	pm->chip_node = of_find_compatible_node(NULL,
		NULL, "mediatek,mt6873-vcodec-dec");
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", 0);
	if (!node) {
		mtk_v4l2_err("of_parse_phandle mediatek,larb fail!");
		return -1;
	}

	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -1;
	}
	pm->larbvdec = &pdev->dev;

	pdev = mtkdev->plat_dev;
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", 1);
	if (!node) {
		mtk_v4l2_err("of_parse_phandle mediatek,larb 1 fail!");
		return -1;
	}

	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -1;
	}
	pm->larbvdec1 = &pdev->dev;

	pdev = mtkdev->plat_dev;
	pm->dev = &pdev->dev;
	if (pm->chip_node) {
		pm->clk_MT_CG_SOC = devm_clk_get(&pdev->dev, "MT_CG_SOC");
		if (IS_ERR(pm->clk_MT_CG_SOC)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_SOC\n");
			return PTR_ERR(pm->clk_MT_CG_SOC);
		}
		pm->clk_MT_CG_VDEC0 = devm_clk_get(&pdev->dev, "MT_CG_VDEC0");
		if (IS_ERR(pm->clk_MT_CG_VDEC0)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VDEC0\n");
			return PTR_ERR(pm->clk_MT_CG_VDEC0);
		}
		pm->clk_MT_CG_VDEC1 = devm_clk_get(&pdev->dev, "MT_CG_VDEC1");
		if (IS_ERR(pm->clk_MT_CG_VDEC1)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VDEC1\n");
			return PTR_ERR(pm->clk_MT_CG_VDEC1);
		}

		pm->clk_MT_CG_VDEC_SEL = devm_clk_get(&pdev->dev, "MT_CG_VDEC_SEL");
		if (IS_ERR(pm->clk_MT_CG_VDEC_SEL)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VDEC1\n");
			return PTR_ERR(pm->clk_MT_CG_VDEC_SEL);
		}

		pm->clk_MT_VDEC_TOP = devm_clk_get(&pdev->dev, "MT_VDEC_TOP");
		if (IS_ERR(pm->clk_MT_VDEC_TOP)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_VDEC_TOP\n");
			return PTR_ERR(pm->clk_MT_VDEC_TOP);
		}

	} else
		mtk_v4l2_err("[VCODEC][ERROR] DTS went wrong...");

	atomic_set(&pm->dec_active_cnt, 0);
	memset(pm->vdec_racing_info, 0, sizeof(pm->vdec_racing_info));
	mutex_init(&pm->dec_racing_info_mutex);

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

	time_check_start(MTK_FMT_DEC, hw_id);
	if (pm->larbvdec) {
		ret = mtk_smi_larb_get(pm->larbvdec);
		if (ret)
			mtk_v4l2_err("Failed to get vdec larb.");
	}

	if (pm->larbvdec1) {
		ret = mtk_smi_larb_get(pm->larbvdec1);
		if (ret)
			mtk_v4l2_err("Failed to get vdec1 larb.");
	}

	ret = clk_prepare_enable(pm->clk_MT_CG_VDEC_SEL);
	if (ret)
		mtk_v4l2_err("clk_prepare_enable clk_MT_CG_VDEC_SEL fail %d",
			ret);
	ret = clk_prepare_enable(pm->clk_MT_VDEC_TOP);
	if (ret)
		mtk_v4l2_err("clk_prepare_enable clk_MT_VDEC_TOP fail %d",
			ret);

	ret = clk_set_parent(pm->clk_MT_CG_VDEC_SEL, pm->clk_MT_VDEC_TOP);
	if (ret)
		mtk_v4l2_err("clk_set_parent fail %d", ret);

	if (hw_id == MTK_VDEC_CORE) {
		ret = clk_prepare_enable(pm->clk_MT_CG_SOC);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable VDEC_SOC fail %d",
				ret);
		ret = clk_prepare_enable(pm->clk_MT_CG_VDEC0);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable VDEC_CORE fail %d",
				ret);
	} else if (hw_id == MTK_VDEC_LAT) {
		ret = clk_prepare_enable(pm->clk_MT_CG_VDEC1);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable VDEC_LAT fail %d",
				ret);
	} else {
		mtk_v4l2_err("invalid hw_id %d", hw_id);
		time_check_end(MTK_FMT_DEC, hw_id, 50);
		return;
	}

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

	if (hw_id == MTK_VDEC_CORE) {
		clk_disable_unprepare(pm->clk_MT_CG_VDEC0);
		clk_disable_unprepare(pm->clk_MT_CG_SOC);
	} else if (hw_id == MTK_VDEC_LAT) {
		clk_disable_unprepare(pm->clk_MT_CG_VDEC1);
	} else
		mtk_v4l2_err("invalid hw_id %d", hw_id);

	if (pm->larbvdec)
		mtk_smi_larb_put(pm->larbvdec);

	if (pm->larbvdec1)
		mtk_smi_larb_put(pm->larbvdec1);

#endif
}

void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
#if DEC_DVFS
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		pr_debug("Failed to get opp table (%d)\n", ret);
		return;
	}

	dev->vdec_reg = devm_regulator_get(&dev->plat_dev->dev,
						"dvfsrc-vcore");
	if (dev->vdec_reg == 0) {
		pr_debug("Failed to get regulator\n");
		return;
	}

	dev->vdec_freq_cnt = dev_pm_opp_get_opp_count(&dev->plat_dev->dev);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq))) {
		dev->vdec_freqs[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
#endif
}

void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
	/* Set to lowest clock before leaving */
}

void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
	int i = 0;
	struct platform_device *pdev = 0;

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VDEC_PORT_NUM; i++)
		dev->vdec_qos_req[i] = 0;

	i = 0;
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_mc");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_ufo");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_pp");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_pred_rd");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_pred_wr");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_ppwrap");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_tile");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_vld");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_vld2");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_avc_mv");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev,
					"path_vdec_rg_ctrl_dma");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_vdec_ufo_enc");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_lat0_vld");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_lat0_vld2");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_lat0_avc_mv");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_lat0_pred_rd");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_lat0_tile");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_lat0_wdma");
	dev->vdec_qos_req[i++] = of_icc_get(&pdev->dev, "path_lat0_rg_ctrl");
#endif
}

void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
#endif
}

void mtk_vdec_dvfs_begin(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	ctx->dev->vdec_freq.freq[hw_id] = 416000000UL;

	if (ctx->dev->dec_cnt > 1)
		ctx->dev->vdec_freq.freq[hw_id] = 546000000UL;

	ctx->dev->vdec_freq.active_freq =
		ctx->dev->vdec_freq.freq[0] > ctx->dev->vdec_freq.freq[1] ?
		ctx->dev->vdec_freq.freq[0] : ctx->dev->vdec_freq.freq[1];

	if (ctx->dev->vdec_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
					&ctx->dev->vdec_freq.active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(ctx->dev->vdec_reg, volt, INT_MAX);
		if (ret) {
			pr_debug("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
	}

	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_dvfs_end(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);

	ctx->dev->vdec_freq.freq[hw_id] = 218000000UL;
	ctx->dev->vdec_freq.active_freq =
		ctx->dev->vdec_freq.freq[0] > ctx->dev->vdec_freq.freq[1] ?
		ctx->dev->vdec_freq.freq[0] : ctx->dev->vdec_freq.freq[1];

	if (ctx->dev->vdec_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
					&ctx->dev->vdec_freq.active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(ctx->dev->vdec_reg, volt, INT_MAX);
		if (ret) {
			pr_debug("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
	}
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_emi_bw_begin(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_EMI_BW
	long emi_bw = 0;
	long emi_bw_input = 0;
	long emi_bw_output = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;
	if (hw_id == MTK_VDEC_LAT) {
		switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_H265:
			emi_bw_input = 31 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			break;
		case V4L2_PIX_FMT_VP9:
		case V4L2_PIX_FMT_AV1:
			emi_bw_input = 13 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			break;
		default:
			emi_bw_input = 31 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
		}

		if (pdev->vdec_qos_req[0] != 0) {
			icc_set_bw(pdev->vdec_qos_req[12],
					Mbps_to_icc(emi_bw_input), 0);
			icc_set_bw(pdev->vdec_qos_req[13], Mbps_to_icc(0), 0);
			icc_set_bw(pdev->vdec_qos_req[14],
					Mbps_to_icc(emi_bw_input * 2), 0);
			icc_set_bw(pdev->vdec_qos_req[15], Mbps_to_icc(10), 0);
			icc_set_bw(pdev->vdec_qos_req[16], Mbps_to_icc(0), 0);
			icc_set_bw(pdev->vdec_qos_req[17],
					Mbps_to_icc(emi_bw_input * 2), 0);
			icc_set_bw(pdev->vdec_qos_req[18], Mbps_to_icc(0), 0);
		}
	} else if (hw_id == MTK_VDEC_CORE) {
		emi_bw = 8L * 1920 * 1080 * 9 * 9 * 5 *
			pdev->vdec_freq.active_freq / 2 / 3;
		emi_bw_output = 1920L * 1088 * 9 * 30 * 9 * 5 *
				pdev->vdec_freq.active_freq /
				4 / 3 / 3 / STD_VDEC_FREQ / 1024 / 1024;

		switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
		case V4L2_PIX_FMT_H264:
			emi_bw_input = 62 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_H265:
			emi_bw_input = 62 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_VP8:
			emi_bw_input = 13 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_VP9:
			emi_bw_input = 26 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_AV1:
			emi_bw_input = 26 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_MPEG4:
		case V4L2_PIX_FMT_H263:
		case V4L2_PIX_FMT_S263:
		case V4L2_PIX_FMT_XVID:
		case V4L2_PIX_FMT_MPEG1:
		case V4L2_PIX_FMT_MPEG2:
			emi_bw_input = 13 * pdev->vdec_freq.active_freq /
					STD_VDEC_FREQ;
			emi_bw = emi_bw * 20 /
					(2 * STD_VDEC_FREQ);
			break;
		}
		emi_bw = emi_bw / (1024 * 1024) / 8;
		emi_bw = emi_bw - emi_bw_output - emi_bw_input;
		if (emi_bw < 0)
			emi_bw = 0;

		if (pdev->vdec_qos_req[0] != 0) {
			if (ctx->picinfo.layout_mode == VDEC_DRV_UFO_AUO_ON) {
				icc_set_bw(pdev->vdec_qos_req[1],
						Mbps_to_icc(emi_bw), 0);
				icc_set_bw(pdev->vdec_qos_req[11],
						Mbps_to_icc(emi_bw), 0);
				icc_set_bw(pdev->vdec_qos_req[0],
						Mbps_to_icc(emi_bw), 0);
			} else {
				icc_set_bw(pdev->vdec_qos_req[0],
						Mbps_to_icc(emi_bw), 0);
				icc_set_bw(pdev->vdec_qos_req[2],
						Mbps_to_icc(emi_bw), 0);
			}
			icc_set_bw(pdev->vdec_qos_req[3],
						Mbps_to_icc(1), 0);
			if ((ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
				V4L2_PIX_FMT_AV1) ||
				(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
				V4L2_PIX_FMT_VP9)) {
				icc_set_bw(pdev->vdec_qos_req[4],
						Mbps_to_icc(emi_bw), 0);
			} else {
				icc_set_bw(pdev->vdec_qos_req[4],
						Mbps_to_icc(1), 0);
			}
			icc_set_bw(pdev->vdec_qos_req[5], Mbps_to_icc(0), 0);
			icc_set_bw(pdev->vdec_qos_req[6], Mbps_to_icc(0), 0);
			icc_set_bw(pdev->vdec_qos_req[7],
					Mbps_to_icc(emi_bw_input), 0);
			icc_set_bw(pdev->vdec_qos_req[8], Mbps_to_icc(0), 0);
			icc_set_bw(pdev->vdec_qos_req[9],
					Mbps_to_icc(emi_bw_input), 0);
			icc_set_bw(pdev->vdec_qos_req[10], Mbps_to_icc(0), 0);
		}
	} else {
		pr_debug("%s unknown hw_id %d\n", __func__, hw_id);
	}

#endif
}

static void mtk_vdec_emi_bw_end(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_EMI_BW
	int i = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;
	if (hw_id == MTK_VDEC_LAT) {
		if (pdev->vdec_qos_req[0] != 0) {
			for (i = 12; i < 19; i++) {
				icc_set_bw(pdev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
			}
		}
	} else if (hw_id == MTK_VDEC_CORE) {
		if (pdev->vdec_qos_req[0] != 0) {
			for (i = 0; i < 12; i++) {
				icc_set_bw(pdev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
			}
		}
	} else {
		pr_debug("%s unknown hw_id %d\n", __func__, hw_id);
	}
#endif
}

void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	/* add_job(&ctx->id, &vdec_jobs); */
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_begin(ctx, hw_id);
	mtk_vdec_emi_bw_begin(ctx, hw_id);
}

void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_end(ctx, hw_id);
	mtk_vdec_emi_bw_end(ctx, hw_id);
}

int mtk_vdec_ion_config_buff(struct dma_buf *dmabuf)
{
/* for dma-buf using ion buffer, ion will check portid in dts
 * So, don't need to config buffer at user side, but remember
 * set iommus attribute in dts file.
 */
#ifdef ion_config_buff
	struct ion_handle *handle = NULL;
	struct ion_mm_data mm_data;

	mtk_v4l2_debug(4, "%p", dmabuf);

	if (!ion_vdec_client)
		ion_vdec_client = ion_client_create(g_ion_device, "vdec");

	handle = ion_import_dma_buf(ion_vdec_client, dmabuf);
	if (IS_ERR(handle)) {
		mtk_v4l2_err("import ion handle failed!\n");
		return -1;
	}
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = M4U_PORT_L4_VDEC_MC_EXT_MDP;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;

	if (ion_kernel_ioctl(ion_vdec_client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data)) {
		mtk_v4l2_err("configure ion buffer failed!\n");
		/* dma hold ref, ion directly free */
		ion_free(ion_vdec_client, handle);

		return -1;
	}

	/* dma hold ref, ion directly free */
	ion_free(ion_vdec_client, handle);
#endif
	return 0;
}

