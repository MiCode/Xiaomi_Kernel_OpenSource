// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <soc/mediatek/smi.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/list.h>
//#include "smi_public.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#if DEC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#define STD_VDEC_FREQ 218000000
#endif

#if DEC_EMI_BW
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#include "vcodec_bw.h"
#endif

//#define VDEC_PRINT_DTS_INFO

static bool mtk_dec_tput_init(struct mtk_vcodec_dev *dev)
{
	const int op_item_num = 7;
	const int tp_item_num = 4;
	const int bw_item_num = 2;
	struct platform_device *pdev;
	int i, j, larb_cnt, ret;
	u32 nmin = 0, nmax = 0, cnt = 0;

	pdev = dev->plat_dev;
	larb_cnt = 0;

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-op-rate-thresh", &nmax);
	if (ret)
		mtk_v4l2_debug(0, "[VDEC] Cannot get op rate thresh, default 0");

	dev->vdec_dvfs_params.per_frame_adjust_op_rate = nmax;
	dev->vdec_dvfs_params.per_frame_adjust = 1;

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-min", &nmin);
	if (ret) {
		nmin = STD_VDEC_FREQ;
		mtk_v4l2_debug(0, "[VDEC] Cannot get min, default %u", nmin);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-normal-max", &nmax);
	if (ret)
		mtk_v4l2_debug(0, "[VDEC] Cannot get normal max, default %u", nmax);

	dev->vdec_dvfs_params.codec_type = MTK_INST_DECODER;
	dev->vdec_dvfs_params.min_freq = nmin;
	dev->vdec_dvfs_params.normal_max_freq = nmax;
	dev->vdec_dvfs_params.allow_oc = 0;

	mtk_v4l2_debug(8, "[VDEC] tput op_th %d, tmin %u, tmax %u",
		dev->vdec_dvfs_params.per_frame_adjust_op_rate,
		dev->vdec_dvfs_params.min_freq,
		dev->vdec_dvfs_params.normal_max_freq);

	/* max operating rate by codec / resolution */
	cnt = of_property_count_u32_elems(pdev->dev.of_node, "max-op-rate-table");
	dev->vdec_op_rate_cnt = cnt / op_item_num;

	mtk_v4l2_debug(8, "[VDEC] max-op-rate table elements %u, %d per line",
			cnt, op_item_num);
	if (!dev->vdec_op_rate_cnt) {
		mtk_v4l2_debug(0, "[VDEC] max-op-rate-table not exist");
		return false;
	}

	dev->vdec_dflt_op_rate = vzalloc(sizeof(struct vcodec_op_rate) * dev->vdec_op_rate_cnt);

	mtk_v4l2_debug(8, "[VDEC] vzalloc %zu x %d res %p",
			sizeof(struct vcodec_op_rate), dev->vdec_op_rate_cnt,
			dev->vdec_dflt_op_rate);
	if (!dev->vdec_dflt_op_rate) {
		mtk_v4l2_debug(0, "[VDEC] vzalloc vdec_dflt_op_rate table failed");
		return false;
	}

	for (i = 0; i < dev->vdec_op_rate_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "max-op-rate-table",
				i * op_item_num, &dev->vdec_dflt_op_rate[i].codec_fmt);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Cannot get default op rate codec_fmt");
			return false;
		}

		for (j = 0; j < (op_item_num - 1) / 2; j++) {
			ret = of_property_read_u32_index(pdev->dev.of_node, "max-op-rate-table",
					i * op_item_num + 1 + j * 2,
					(u32 *)&dev->vdec_dflt_op_rate[i].pixel_per_frame[j]);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get pixel per frame %d %d",
						i, j);
				return false;
			}

			ret = of_property_read_u32_index(pdev->dev.of_node, "max-op-rate-table",
					i * op_item_num + 2 + j * 2,
					(u32 *)&dev->vdec_dflt_op_rate[i].max_op_rate[j]);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get max_op_rate %d %d",
						i, j);
				return false;
			}
		}
		dev->vdec_dflt_op_rate[i].codec_type = 0;
	}


	/* throughput */
	cnt = of_property_count_u32_elems(pdev->dev.of_node, "throughput-table");
	dev->vdec_tput_cnt = cnt / tp_item_num;

	mtk_v4l2_debug(8, "[VDEC] tput table elements %u, %d per line",
			cnt, tp_item_num, dev->vdec_tput);
	if (!dev->vdec_tput_cnt) {
		mtk_v4l2_debug(0, "[VDEC] throughtput table not exist");
		return false;
	}

	dev->vdec_tput = vzalloc(sizeof(struct vcodec_perf) * dev->vdec_tput_cnt);

	mtk_v4l2_debug(8, "[VDEC] vzalloc %zu x %d res %p",
			sizeof(struct vcodec_perf), dev->vdec_tput_cnt, dev->vdec_tput);
	if (!dev->vdec_tput) {
		mtk_v4l2_debug(0, "[VDEC] vzalloc vdec_tput table failed");
		return false;
	}

	for (i = 0; i < dev->vdec_tput_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num, &dev->vdec_tput[i].codec_fmt);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Cannot get codec_fmt");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 1, (u32 *)&dev->vdec_tput[i].config);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Cannot get config");
			return false;
		}


		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 2, &dev->vdec_tput[i].cy_per_mb_1);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Cannot get cycle per mb 1");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 3, &dev->vdec_tput[i].cy_per_mb_2);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Cannot get cycle per mb 2");
			return false;
		}
		dev->vdec_tput[i].codec_type = 0;
	}

	/* bw */
	dev->vdec_port_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"bandwidth-table") / bw_item_num;

	if (dev->vdec_port_cnt > MTK_VDEC_PORT_NUM) {
		mtk_v4l2_debug(0, "[VDEC] vdec port over limit %d > %d",
			dev->vdec_port_cnt, MTK_VDEC_PORT_NUM);
		dev->vdec_port_cnt = MTK_VDEC_PORT_NUM;
	}

	if (!dev->vdec_port_cnt) {
		mtk_v4l2_debug(0, "[VDEC] bandwidth table not exist");
		return false;
	}

	dev->vdec_port_bw = vzalloc(sizeof(struct vcodec_port_bw) * dev->vdec_port_cnt);
	if (!dev->vdec_port_bw) {
		/* mtk_v4l2_debug(0, "[VDEC] vzalloc vdec_port_bw table failed"); */
		return false;
	}

	for (i = 0; i < dev->vdec_port_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
				i * bw_item_num, (u32 *)&dev->vdec_port_bw[i].port_type);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Cannot get bw port type");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
				i * bw_item_num + 1, &dev->vdec_port_bw[i].port_base_bw);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Cannot get base bw");
			return false;
		}

		/* larb port sum placeholder */
		if (dev->vdec_port_bw[i].port_type == VCODEC_PORT_LARB_SUM) {
			dev->vdec_port_bw[i].larb = dev->vdec_port_bw[i].port_base_bw;
			dev->vdec_port_bw[i].port_base_bw = 0;
			if (i + 1 < dev->vdec_port_cnt)
				dev->vdec_port_idx[++larb_cnt] = i + 1;
		}
	}
#ifdef VDEC_PRINT_DTS_INFO
	mtk_v4l2_debug(0, "[VDEC] tput_cnt %d, cfg_cnt %d, port_cnt %d",
		dev->vdec_tput_cnt, dev->vdec_cfg_cnt, dev->vdec_port_cnt);

	for (i = 0; i < dev->vdec_op_rate_cnt; i++) {
		mtk_v4l2_debug(0, "[VDEC] oprate fmt %u, %u/%u,%u/%u,%u/%u,%u/%u",
			dev->vdec_dflt_op_rate[i].codec_fmt,
			dev->vdec_dflt_op_rate[i].pixel_per_frame[0],
			dev->vdec_dflt_op_rate[i].max_op_rate[0],
			dev->vdec_dflt_op_rate[i].pixel_per_frame[1],
			dev->vdec_dflt_op_rate[i].max_op_rate[1],
			dev->vdec_dflt_op_rate[i].pixel_per_frame[2],
			dev->vdec_dflt_op_rate[i].max_op_rate[2],
			dev->vdec_dflt_op_rate[i].pixel_per_frame[3],
			dev->vdec_dflt_op_rate[i].max_op_rate[3]);
	}

	for (i = 0; i < dev->vdec_tput_cnt; i++) {
		mtk_v4l2_debug(0, "[VDEC] tput fmt %u, cfg %d, cy1 %u, cy2 %u",
			dev->vdec_tput[i].codec_fmt,
			dev->vdec_tput[i].config,
			dev->vdec_tput[i].cy_per_mb_1,
			dev->vdec_tput[i].cy_per_mb_2);
	}

	for (i = 0; i < dev->vdec_port_cnt; i++) {
		mtk_v4l2_debug(0, "[VDEC] port[%d] type %d, bw %u, larb %u", i,
			dev->vdec_port_bw[i].port_type,
			dev->vdec_port_bw[i].port_base_bw,
			dev->vdec_port_bw[i].larb);
	}
#endif
	return true;
}

static void mtk_dec_tput_deinit(struct mtk_vcodec_dev *dev)
{
	if (dev->vdec_dflt_op_rate) {
		vfree(dev->vdec_dflt_op_rate);
		dev->vdec_dflt_op_rate = 0;
	}

	if (dev->vdec_tput) {
		vfree(dev->vdec_tput);
		dev->vdec_tput = 0;
	}

	if (dev->vdec_port_bw) {
		vfree(dev->vdec_port_bw);
		dev->vdec_port_bw = 0;
	}
}


void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
#if DEC_DVFS
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;
	bool tput_ret = false;

	INIT_LIST_HEAD(&dev->vdec_dvfs_inst);

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		dev->vdec_reg = 0;
		mtk_v4l2_debug(0, "[VDEC] Failed to get opp table (%d)", ret);
		return;
	}

	dev->vdec_reg = devm_regulator_get(&dev->plat_dev->dev,
						"dvfsrc-vcore");
	if (dev->vdec_reg == 0) {
		mtk_v4l2_debug(0, "[VDEC] Failed to get regulator");
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

	tput_ret = mtk_dec_tput_init(dev);
#endif
}

void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
#if DEC_DVFS
	/* Set to lowest clock before leaving */
	mtk_dec_tput_deinit(dev);
#endif
}

void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
	int i, ret;
	struct platform_device *pdev = 0;
	u32 port_num = 0;
	const char *path_strs[32];

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VDEC_PORT_NUM; i++)
		dev->vdec_qos_req[i] = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "interconnect-num", &port_num);
	if (ret) {
		mtk_v4l2_debug(0, "[VDEC] Cannot get interconnect num, skip");
		return;
	}

	ret = of_property_read_string_array(pdev->dev.of_node, "interconnect-names",
						path_strs, port_num);

	if (ret < 0) {
		mtk_v4l2_debug(0, "[VDEC] Cannot get interconnect names, skip");
		return;
	} else if (ret != (int)port_num) {
		mtk_v4l2_debug(0, "[VDEC] Interconnect name count not match %u %d",
			port_num, ret);
	}

	for (i = 0; i < port_num; i++) {
		dev->vdec_qos_req[i] = of_mtk_icc_get(&pdev->dev, path_strs[i]);
		mtk_v4l2_debug(16, "[VDEC] qos port[%d] name %s", i, path_strs[i]);
	}
#endif
}

void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
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

		mtk_v4l2_debug(8, "[VDEC] freq %u, voltage %d", freq, volt);

		ret = regulator_set_voltage(dev->vdec_reg, volt, INT_MAX);
		if (ret) {
			mtk_v4l2_debug(0, "[VDEC] Failed to set regulator voltage %d\n", volt);
		}
	}
}

void mtk_vdec_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(8, "[VDEC] ctx = %p",  ctx);

	if (need_update(ctx)) {
		update_freq(ctx->dev, MTK_INST_DECODER);
		mtk_v4l2_debug(4, "[VDEC] freq %u", ctx->dev->vdec_dvfs_params.target_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.target_freq);
	}
}

void mtk_vdec_dvfs_end_inst(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(8, "[VDEC] ctx = %p",  ctx);

	if (remove_update(ctx)) {
		update_freq(ctx->dev, MTK_INST_DECODER);
		mtk_v4l2_debug(4, "[VDEC] freq %u", ctx->dev->vdec_dvfs_params.target_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.target_freq);
	}
}

void mtk_vdec_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u64 target_bw = 0;

	mtk_v4l2_debug(8, "[VDEC] ctx = %p",  ctx);
	dev = ctx->dev;
	if (dev->vdec_reg == 0)
		return;

	for (i = 0; i < dev->vdec_port_cnt; i++) {
		target_bw = div_64(
			(u64)dev->vdec_port_bw[i].port_base_bw * dev->vdec_dvfs_params.target_freq,
			dev->vdec_dvfs_params.min_freq);
		if (dev->vdec_port_bw[i].port_type < VCODEC_PORT_LARB_SUM) {
			if (dev->vdec_dvfs_params.target_freq == dev->vdec_dvfs_params.min_freq) {
				mtk_icc_set_bw_not_update(dev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
				mtk_v4l2_debug(8, "[VDEC] port %d bw %lu (0)MB/s",
					i, (u64)target_bw);
			} else {
				mtk_icc_set_bw_not_update(dev->vdec_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
				mtk_v4l2_debug(8, "[VDEC] port %d bw %lu MB/s", i, (u64)target_bw);
			}
		} else if (dev->vdec_port_bw[i].port_type == VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw(dev->vdec_qos_req[i], 0, 0);
			mtk_v4l2_debug(8, "[VDEC] port %d set larb %u bw",
					i, dev->vdec_port_bw[i].larb);
		} else {
			mtk_v4l2_debug(8, "[VDEC] unknown port type %d %d\n",
				i, dev->vdec_port_bw[i].port_type);
		}
	}
}

void mtk_vdec_pmqos_end_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u64 target_bw = 0;

	mtk_v4l2_debug(8, "[VDEC] ctx = %p",  ctx);
	dev = ctx->dev;
	if (dev->vdec_reg == 0)
		return;

	for (i = 0; i < dev->vdec_port_cnt; i++) {
		target_bw = div_64(
			(u64)dev->vdec_port_bw[i].port_base_bw * dev->vdec_dvfs_params.target_freq,
			dev->vdec_dvfs_params.min_freq);

		if (list_empty(&dev->vdec_dvfs_inst)) /* no more instances */
			target_bw = 0;

		if (dev->vdec_port_bw[i].port_type < VCODEC_PORT_LARB_SUM) {
			if (dev->vdec_dvfs_params.target_freq == dev->vdec_dvfs_params.min_freq) {
				mtk_icc_set_bw_not_update(dev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
				mtk_v4l2_debug(8, "[VDEC] port %d bw %lu (0)MB/s",
					i, (u64)target_bw);
			} else {
				mtk_icc_set_bw_not_update(dev->vdec_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
				mtk_v4l2_debug(8, "[VDEC] port %d bw %lu MB/s", i, (u64)target_bw);
			}
		} else if (dev->vdec_port_bw[i].port_type == VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw(dev->vdec_qos_req[i], 0, 0);
			mtk_v4l2_debug(8, "[VDEC] port %d set larb %u bw",
					i, dev->vdec_port_bw[i].larb);
		} else {
			mtk_v4l2_debug(8, "[VDEC] unknown port type %d",
				dev->vdec_port_bw[i].port_type);
		}
	}
}


void mtk_vdec_dvfs_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	struct mtk_vcodec_dev *dev = 0;

	dev = ctx->dev;
	if (dev->vdec_reg == 0)
		return;

	dev->vdec_dvfs_params.frame_need_update = 1;

	if (!dev->vdec_dvfs_params.per_frame_adjust)
		dev->vdec_dvfs_params.frame_need_update = 0;

	if (dev->vdec_dvfs_params.frame_need_update &&
		(dev->vdec_dvfs_params.target_freq != dev->vdec_dvfs_params.min_freq)) {
		mtk_v4l2_debug(4, "[VDEC] f_begin freq %u",
			ctx->dev->vdec_dvfs_params.target_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.target_freq);
	}
}


void mtk_vdec_dvfs_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	struct mtk_vcodec_dev *dev = 0;

	dev = ctx->dev;
	if (dev->vdec_reg == 0)
		return;

	dev->vdec_dvfs_params.frame_need_update = 1;

	if (!dev->vdec_dvfs_params.per_frame_adjust)
		dev->vdec_dvfs_params.frame_need_update = 0;

	if (dev->vdec_dvfs_params.frame_need_update &&
		(dev->vdec_dvfs_params.target_freq != dev->vdec_dvfs_params.min_freq)) {
		mtk_v4l2_debug(4, "[VDEC] f_end freq %u", ctx->dev->vdec_dvfs_params.min_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.min_freq);
	}
}


void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = 0;

	dev = ctx->dev;
	if (dev->vdec_reg == 0)
		return;

	if (dev->vdec_dvfs_params.frame_need_update &&
		(dev->vdec_dvfs_params.target_freq != dev->vdec_dvfs_params.min_freq)) {
		mtk_vdec_pmqos_begin_inst(ctx);
	}
	dev->vdec_dvfs_params.frame_need_update = 0;
}


void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u64 target_bw = 0;

	dev = ctx->dev;
	if (dev->vdec_reg == 0)
		return;

	if (!dev->vdec_dvfs_params.frame_need_update ||
		(dev->vdec_dvfs_params.target_freq == dev->vdec_dvfs_params.min_freq))
		return;

	for (i = 0; i < dev->vdec_port_cnt; i++) {
		target_bw = 0;

		if (dev->vdec_port_bw[i].port_type < VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw_not_update(dev->vdec_qos_req[i],
				MBps_to_icc((u32)target_bw), 0);
			mtk_v4l2_debug(8, "[VDEC] port %d bw %lu MB/s", i, (u64)target_bw);
		} else if (dev->vdec_port_bw[i].port_type == VCODEC_PORT_LARB_SUM) {
			mtk_icc_set_bw(dev->vdec_qos_req[i], 0, 0);
			mtk_v4l2_debug(8, "[VDEC] port %d set larb %u bw",
					i, dev->vdec_port_bw[i].larb);
		} else {
			mtk_v4l2_debug(8, "[VDEC] unknown port type %d",
				dev->vdec_port_bw[i].port_type);
		}
	}
	dev->vdec_dvfs_params.frame_need_update = 0;
}



