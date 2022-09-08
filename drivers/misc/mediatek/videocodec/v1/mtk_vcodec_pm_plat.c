// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <soc/mediatek/smi.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "mtk_vcodec_pm.h"
#include "mtk_vcodec_pm_plat.h"


#define VCODEC_DVFS_V2	1
#define VCODEC_EMI_BW	1
#define USE_WAKELOCK 0

#if VCODEC_DVFS_V2
#define STD_VDEC_FREQ 320
#define STD_VENC_FREQ 320
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "dvfs_v2.h"
#endif

#if VCODEC_EMI_BW
#include "mtk-interconnect.h"
//#include "vcodec_bw.h"
#endif




void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if VCODEC_DVFS_V2
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		dev->venc_reg = 0;
		pr_info("[VENC] Failed to get opp table (%d)", ret);
		return;
	}

	dev->venc_reg = devm_regulator_get(&dev->plat_dev->dev,
					"dvfsrc-vcore");
	if (dev->venc_reg == 0) {
		pr_info("[VENC] Failed to get regulator");
		return;
	}

	dev->venc_freq_cnt = dev_pm_opp_get_opp_count(&dev->plat_dev->dev);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq))) {
		dev->venc_freqs[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
#endif
}

void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if VCODEC_DVFS_V2
#endif
}

void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if VCODEC_EMI_BW
	int i, ret;
	struct platform_device *pdev = 0;
	u32 port_num = 0;
	const char *path_strs[64];

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VENC_PORT_NUM; i++)
		dev->venc_qos_req[i] = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "interconnect-num", &port_num);
	if (ret) {
		pr_info("[VENC] Cannot get interconnect num, skip");
		return;
	}

	ret = of_property_read_string_array(pdev->dev.of_node, "interconnect-names",
					path_strs, port_num);

	if (ret < 0) {
		pr_info("[VENC] Cannot get interconnect names, skip");
		return;
	} else if (ret != (int)port_num) {
		pr_info("[VENC] Interconnect name count not match %u %d", port_num, ret);
	}

	if (port_num > MTK_VENC_PORT_NUM) {
		pr_info("[VENC] venc port over limit %u > %d",
			port_num, MTK_VENC_PORT_NUM);
		port_num = MTK_VENC_PORT_NUM;
	}

	for (i = 0; i < port_num; i++) {
		dev->venc_qos_req[i] = of_mtk_icc_get(&pdev->dev, path_strs[i]);
		pr_info("[VENC] %d %p %s", i, dev->venc_qos_req[i], path_strs[i]);
	}
#endif
}

void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if VCODEC_EMI_BW
#endif
}


void set_venc_opp(struct mtk_vcodec_dev *dev, u32 freq)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long freq_64 = (unsigned long)freq;

	if (dev->venc_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq_64);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		pr_info("[VENC] freq %lu, voltage %d", freq, volt);

		ret = regulator_set_voltage(dev->venc_reg, volt, INT_MAX);
		if (ret)
			pr_info("[VENC] Failed to set regulator voltage %d", volt);
	}

}

void set_venc_bw(struct mtk_vcodec_dev *dev, u64 target_bw)
{
	int i;

	if (dev->venc_reg == 0)
		return;

	for (i = 0; i < MTK_VENC_PORT_NUM; i++) {
		mtk_icc_set_bw(dev->venc_qos_req[i], MBps_to_icc((u32)target_bw), 0);
		pr_info("[VENC] port %d set larb %u bw",
			i, target_bw);
	}
}


void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
#if VCODEC_DVFS_V2
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		dev->vdec_reg = 0;
		pr_info("[VDEC] Failed to get opp table (%d)", ret);
		return;
	}

	dev->vdec_reg = devm_regulator_get(&dev->plat_dev->dev,
					"dvfsrc-vcore");
	if (dev->vdec_reg == 0) {
		pr_info("[VDEC] Failed to get regulator");
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
#if VCODEC_DVFS_V2

#endif
}

void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if VCODEC_EMI_BW
	int i, ret;
	struct platform_device *pdev = 0;
	u32 port_num = 0;
	const char *path_strs[32];

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VDEC_PORT_NUM; i++)
		dev->vdec_qos_req[i] = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "interconnect-num", &port_num);
	if (ret) {
		pr_info("[VDEC] Cannot get interconnect num, skip");
		return;
	}

	ret = of_property_read_string_array(pdev->dev.of_node, "interconnect-names",
						path_strs, port_num);

	if (ret < 0) {
		pr_info("[VDEC] Cannot get interconnect names, skip");
		return;
	} else if (ret != (int)port_num) {
		pr_info("[VDEC] Interconnect name count not match %u %d",
			port_num, ret);
	}

	for (i = 0; i < port_num; i++) {
		dev->vdec_qos_req[i] = of_mtk_icc_get(&pdev->dev, path_strs[i]);
		pr_info("[VDEC] qos port[%d] name %s", i, path_strs[i]);
	}
#endif
}

void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if VCODEC_EMI_BW
#endif
}

void set_vdec_opp(struct mtk_vcodec_dev *dev, u32 freq)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long freq_64 = (unsigned long)freq;

	if (dev->vdec_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq_64);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		pr_info("[VDEC] freq %u, voltage %d", freq, volt);

		ret = regulator_set_voltage(dev->vdec_reg, volt, INT_MAX);
		if (ret)
			pr_info("[VDEC] Failed to set regulator voltage %d\n", volt);
	}

}

void set_vdec_bw(struct mtk_vcodec_dev *dev, u64 target_bw)
{
	int i;

	if (dev->vdec_reg == 0)
		return;
	for (i = 0; i < MTK_VDEC_PORT_NUM; i++) {
		mtk_icc_set_bw(dev->vdec_qos_req[i], MBps_to_icc((u32)target_bw), 0);
		pr_info("[VDEC] port %d set larb %u bw",
			i, target_bw);
	}
}

