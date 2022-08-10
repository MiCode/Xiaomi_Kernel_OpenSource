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

#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_enc_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#define USE_GCE 0
#if ENC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 250000000
#endif

#if ENC_EMI_BW
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#include "vcodec_bw.h"
#endif

//#define VENC_PRINT_DTS_INFO

static bool mtk_enc_tput_init(struct mtk_vcodec_dev *dev)
{
	const int tp_item_num = 4;
	const int cfg_item_num = 4;
	const int bw_item_num = 2;
	int i, larb_cnt, ret;
	struct platform_device *pdev;
	u32 nmin = 0, nmax = 0;
	s32 offset = 0;

	pdev = dev->plat_dev;
	larb_cnt = 0;

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-op-rate-thresh", &nmax);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] Cannot get op rate thresh, default 0");

	dev->venc_dvfs_params.per_frame_adjust_op_rate = nmax;
	dev->venc_dvfs_params.per_frame_adjust = 1;

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-min", &nmin);
	if (ret) {
		nmin = STD_VENC_FREQ;
		mtk_v4l2_debug(0, "[VENC] Cannot get min, default %u", nmin);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-normal-max", &nmax);
	if (ret) {
		nmax = STD_VENC_FREQ;
		mtk_v4l2_debug(0, "[VENC] Cannot get normal max, default %u", nmax);
	}
	dev->venc_dvfs_params.codec_type = MTK_INST_ENCODER;
	dev->venc_dvfs_params.min_freq = nmin;
	dev->venc_dvfs_params.normal_max_freq = nmax;
	dev->venc_dvfs_params.allow_oc = 0;

	/* throughput */
	dev->venc_tput_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"throughput-table") / tp_item_num;

	if (!dev->venc_tput_cnt) {
		mtk_v4l2_debug(0, "[VENC] throughput table not exist");
		return false;
	}

	dev->venc_tput = vzalloc(sizeof(struct vcodec_perf) * dev->venc_tput_cnt);
	if (!dev->venc_tput) {
		/* mtk_v4l2_debug(0, "[VENC] vzalloc venc_tput table failed"); */
		return false;
	}

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-config-offset", &offset);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] Cannot get config-offset, default 0");

	for (i = 0; i < dev->venc_tput_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num, &dev->venc_tput[i].codec_fmt);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get codec_fmt");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 1, (u32 *)&dev->venc_tput[i].config);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get config");
			return false;
		}
		dev->venc_tput[i].config -= offset;

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 2, &dev->venc_tput[i].cy_per_mb_1);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get cycle per mb 1");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 3, &dev->venc_tput[i].cy_per_mb_2);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get cycle per mb 2");
			return false;
		}
		dev->venc_tput[i].codec_type = 1;
	}

	/* config */
	dev->venc_cfg_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"config-table") / cfg_item_num;

	if (!dev->venc_cfg_cnt) {
		mtk_v4l2_debug(0, "[VENC] config table not exist");
		return false;
	}

	dev->venc_cfg = vzalloc(sizeof(struct vcodec_config) * dev->venc_cfg_cnt);
	if (!dev->venc_cfg) {
		/* mtk_v4l2_debug(0, "[VENC] vzalloc venc_cfg table failed"); */
		return false;
	}

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-config-offset", &offset);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] Cannot get config-offset, default 0");

	for (i = 0; i < dev->venc_cfg_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num, &dev->venc_cfg[i].codec_fmt);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get cfg codec_fmt");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num + 1, (u32 *)&dev->venc_cfg[i].mb_thresh);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get mb_thresh");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num + 2, &dev->venc_cfg[i].config_1);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get config 1");
			return false;
		}
		dev->venc_cfg[i].config_1 -= offset;

		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num + 3, &dev->venc_cfg[i].config_2);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get config 2");
			return false;
		}
		dev->venc_cfg[i].config_2 -= offset;
		dev->venc_cfg[i].codec_type = 1;
	}

	/* bw */
	dev->venc_port_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"bandwidth-table") / bw_item_num;

	if (dev->venc_port_cnt > MTK_VENC_PORT_NUM) {
		mtk_v4l2_debug(0, "[VENC] venc port over limit %d > %d",
				dev->venc_port_cnt, MTK_VENC_PORT_NUM);
		dev->venc_port_cnt = MTK_VENC_PORT_NUM;
	}

	if (!dev->venc_port_cnt) {
		mtk_v4l2_debug(0, "[VENC] bandwidth table not exist");
		return false;
	}

	dev->venc_port_bw = vzalloc(sizeof(struct vcodec_port_bw) * dev->venc_port_cnt);
	if (!dev->venc_port_bw) {
		/* mtk_v4l2_debug(0, "[VENC] vzalloc venc_port_bw table failed"); */
		return false;
	}

	for (i = 0; i < dev->venc_port_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
				i * bw_item_num, (u32 *)&dev->venc_port_bw[i].port_type);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get bw port type");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
				i * bw_item_num + 1, &dev->venc_port_bw[i].port_base_bw);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get base bw");
			return false;
		}

		/* larb port sum placeholder */
		if (dev->venc_port_bw[i].port_type == VCODEC_PORT_LARB_SUM) {
			dev->venc_port_bw[i].larb = dev->venc_port_bw[i].port_base_bw;
			dev->venc_port_bw[i].port_base_bw = 0;
			if (i + 1 < dev->venc_port_cnt)
				dev->venc_port_idx[++larb_cnt] = i + 1;
		}
	}

#ifdef VENC_PRINT_DTS_INFO
	mtk_v4l2_debug(0, "[VENC] tput_cnt %d, cfg_cnt %d, port_cnt %d\n",
		dev->venc_tput_cnt, dev->venc_cfg_cnt, dev->venc_port_cnt);

	for (i = 0; i < dev->venc_tput_cnt; i++) {
		mtk_v4l2_debug(0, "[VENC] tput fmt %u, cfg %d, cy1 %u, cy2 %u",
			dev->venc_tput[i].codec_fmt,
			dev->venc_tput[i].config,
			dev->venc_tput[i].cy_per_mb_1,
			dev->venc_tput[i].cy_per_mb_2);
	}

	for (i = 0; i < dev->venc_cfg_cnt; i++) {
		mtk_v4l2_debug(0, "[VENC] config fmt %u, mb_thresh %u, cfg1 %d, cfg2 %d",
			dev->venc_cfg[i].codec_fmt,
			dev->venc_cfg[i].mb_thresh,
			dev->venc_cfg[i].config_1,
			dev->venc_cfg[i].config_2);
	}

	for (i = 0; i < dev->venc_port_cnt; i++) {
		mtk_v4l2_debug(0, "[VENC] port[%d] type %d, bw %u, larb %u", i,
			dev->venc_port_bw[i].port_type,
			dev->venc_port_bw[i].port_base_bw,
			dev->venc_port_bw[i].larb);
	}
#endif
	return true;
}

static void mtk_enc_tput_deinit(struct mtk_vcodec_dev *dev)
{
	if (dev->venc_tput) {
		vfree(dev->venc_tput);
		dev->venc_tput = 0;
	}

	if (dev->venc_cfg) {
		vfree(dev->venc_cfg);
		dev->venc_cfg = 0;
	}

	if (dev->venc_port_bw) {
		vfree(dev->venc_port_bw);
		dev->venc_port_bw = 0;
	}
}

void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if ENC_DVFS
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;
	bool tput_ret;

	INIT_LIST_HEAD(&dev->venc_dvfs_inst);

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		dev->venc_reg = 0;
		mtk_v4l2_debug(0, "[VENC] Failed to get opp table (%d)", ret);
		return;
	}

	dev->venc_reg = devm_regulator_get(&dev->plat_dev->dev,
						"dvfsrc-vcore");
	if (dev->venc_reg == 0) {
		mtk_v4l2_debug(0, "[VENC] Failed to get regulator");
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

	tput_ret = mtk_enc_tput_init(dev);
#endif
}

void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if ENC_DVFS
	mtk_enc_tput_deinit(dev);
#endif
}

void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if ENC_EMI_BW
	int i, ret;
	struct platform_device *pdev = 0;
	u32 port_num = 0;
	const char *path_strs[64];

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VENC_PORT_NUM; i++)
		dev->venc_qos_req[i] = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "interconnect-num", &port_num);
	if (ret) {
		mtk_v4l2_debug(0, "[VENC] Cannot get interconnect num, skip");
		return;
	}

	ret = of_property_read_string_array(pdev->dev.of_node, "interconnect-names",
						path_strs, port_num);

	if (ret < 0) {
		mtk_v4l2_debug(0, "[VENC] Cannot get interconnect names, skip");
		return;
	} else if (ret != (int)port_num) {
		mtk_v4l2_debug(0, "[VENC] Interconnect name count not match %u %d", port_num, ret);
	}

	if (port_num > MTK_VENC_PORT_NUM) {
		mtk_v4l2_debug(0, "[VENC] venc port over limit %u > %d",
				port_num, MTK_VENC_PORT_NUM);
		port_num = MTK_VENC_PORT_NUM;
	}

	for (i = 0; i < port_num; i++) {
		dev->venc_qos_req[i] = of_mtk_icc_get(&pdev->dev, path_strs[i]);
		mtk_v4l2_debug(16, "[VENC] %d %p %s", i, dev->venc_qos_req[i], path_strs[i]);
	}
#endif
}

void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if ENC_EMI_BW
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

		mtk_v4l2_debug(8, "[VENC] freq %u, voltage %d", freq, volt);

		ret = regulator_set_voltage(dev->venc_reg, volt, INT_MAX);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Failed to set regulator voltage %d", volt);
		}
	}
}

void mtk_venc_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(8, "[VENC] ctx = %p",  ctx);

	if (need_update(ctx)) {
		update_freq(ctx->dev, MTK_INST_ENCODER);
		mtk_v4l2_debug(4, "[VENC] freq %u", ctx->dev->venc_dvfs_params.target_freq);
		set_venc_opp(ctx->dev, ctx->dev->venc_dvfs_params.target_freq);
	}
}

void mtk_venc_dvfs_end_inst(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(8, "[VENC] ctx = %p",  ctx);

	if (remove_update(ctx)) {
		update_freq(ctx->dev, MTK_INST_ENCODER);
		mtk_v4l2_debug(4, "[VENC] freq %u", ctx->dev->venc_dvfs_params.target_freq);
		set_venc_opp(ctx->dev, ctx->dev->venc_dvfs_params.target_freq);
	}
}

void mtk_venc_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u64 target_bw = 0;

	dev = ctx->dev;
	if (dev->venc_reg == 0)
		return;

	if ((dev->venc_dvfs_params.target_freq == dev->venc_dvfs_params.min_freq) &&
		(dev->venc_dvfs_params.target_freq >
		(dev->venc_dvfs_params.freq_sum * 3))) {
		mtk_v4l2_debug(8, "[VENC] Loading too low %u / %u, 0 QoS BW",
			dev->venc_dvfs_params.freq_sum,
			dev->venc_dvfs_params.target_freq);
		return;
	}

	for (i = 0; i < dev->venc_port_cnt; i++) {
		target_bw = (u64)dev->venc_port_bw[i].port_base_bw *
			dev->venc_dvfs_params.target_freq /
			dev->venc_dvfs_params.min_freq;
		if (dev->venc_port_bw[i].port_type < VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw_not_update(dev->venc_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
			mtk_v4l2_debug(8, "[VENC] port %d bw %u MB/s", i, (u32)target_bw);
		} else if (dev->venc_port_bw[i].port_type == VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw(dev->venc_qos_req[i], 0, 0);
			mtk_v4l2_debug(8, "[VENC] port %d set larb %u bw",
					i, dev->venc_port_bw[i].larb);
		} else {
			mtk_v4l2_debug(8, "[VENC] unknown port type %d\n",
					dev->venc_port_bw[i].port_type);
		}
	}
}

void mtk_venc_pmqos_end_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u64 target_bw = 0;

	dev = ctx->dev;
	if (dev->venc_reg == 0)
		return;

	for (i = 0; i < dev->venc_port_cnt; i++) {
		target_bw = (u64)dev->venc_port_bw[i].port_base_bw *
			dev->venc_dvfs_params.target_freq /
			dev->venc_dvfs_params.min_freq;

		if (list_empty(&dev->venc_dvfs_inst)) /* no more instances */
			target_bw = 0;

		if (dev->venc_port_bw[i].port_type < VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw_not_update(dev->venc_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
			mtk_v4l2_debug(8, "[VENC] port %d bw %u MB/s", i, (u32)target_bw);
		} else if (dev->venc_port_bw[i].port_type == VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw(dev->venc_qos_req[i], 0, 0);
			mtk_v4l2_debug(8, "[VENC] port %d set larb %u bw",
					i, dev->venc_port_bw[i].larb);
		} else {
			mtk_v4l2_debug(8, "[VENC] unknown port type %d\n",
					dev->venc_port_bw[i].port_type);
		}
	}
}
