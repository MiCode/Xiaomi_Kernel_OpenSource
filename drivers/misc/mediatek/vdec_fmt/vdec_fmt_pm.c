// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "vdec_fmt_pm.h"
#include "vdec_fmt_dmabuf.h"
#include "vdec_fmt_utils.h"

#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <soc/mediatek/smi.h>
#include "mtk-interconnect.h"
#include <linux/timekeeping.h>

void fmt_get_module_clock_by_name(struct mtk_vdec_fmt *fmt,
	const char *clkName, struct clk **clk_module)
{
	*clk_module = of_clk_get_by_name(fmt->dev->of_node, clkName);
	if (IS_ERR(*clk_module)) {
		fmt_err("cannot get module clock:%s", clkName);
		*clk_module = NULL;
	} else
		fmt_debug(0, "get module clock:%s", clkName);
}

/* Common Clock Framework */
void fmt_init_pm(struct mtk_vdec_fmt *fmt)
{
	fmt_debug(0, "+");
	fmt_get_module_clock_by_name(fmt, "MT_CG_VDEC",
		&fmt->clk_VDEC);
	fmt_get_module_clock_by_name(fmt, "MT_CG_MINI_MDP",
		&fmt->clk_MINI_MDP);
	pm_runtime_enable(fmt->dev);
}

int32_t fmt_clock_on(struct mtk_vdec_fmt *fmt)
{
	int ret = 0;
	s32 cmdq_ret = 0;

	cmdq_ret = cmdq_mbox_enable(fmt->clt_fmt[0]->chan);
	while (cmdq_ret > 1)
		cmdq_ret = cmdq_mbox_disable(fmt->clt_fmt[0]->chan);
	if (fmt->fmtLarb) {
		ret = mtk_smi_larb_get(fmt->fmtLarb);
		if (ret) {
			fmt_debug(0, "mtk_smi_larb_get failed %d",
				ret);
			return ret;
		}
	}
	if (fmt->clk_VDEC) {
		ret = clk_prepare_enable(fmt->clk_VDEC);
		if (ret)
			fmt_debug(0, "clk_prepare_enable VDEC_SOC failed %d", ret);
	}
	if (fmt->clk_MINI_MDP) {
		ret = clk_prepare_enable(fmt->clk_MINI_MDP);
		if (ret)
			fmt_debug(0, "clk_prepare_enable VDEC_MINI_MDP failed %d", ret);
	}
	cmdq_util_prebuilt_init(CMDQ_PREBUILT_VFMT);
	return ret;
}

int32_t fmt_clock_off(struct mtk_vdec_fmt *fmt)
{
	s32 cmdq_ret = 0;

	if (fmt->clk_MINI_MDP)
		clk_disable_unprepare(fmt->clk_MINI_MDP);
	if (fmt->clk_VDEC)
		clk_disable_unprepare(fmt->clk_VDEC);
	if (fmt->fmtLarb)
		mtk_smi_larb_put(fmt->fmtLarb);
	cmdq_ret = cmdq_mbox_disable(fmt->clt_fmt[0]->chan);
	while (cmdq_ret > 0)
		cmdq_ret = cmdq_mbox_disable(fmt->clt_fmt[0]->chan);
	atomic_set(&fmt->fmt_error, 0);
	return 0;
}

void fmt_prepare_dvfs_emi_bw(struct mtk_vdec_fmt *fmt)
{
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(fmt->dev);
	if (ret < 0) {
		fmt_debug(0, "Failed to get opp table (%d)\n", ret);
		return;
	}

	fmt->fmt_reg = devm_regulator_get(fmt->dev,
						"dvfsrc-vcore");
	if (fmt->fmt_reg == 0) {
		fmt_debug(0, "Failed to get regulator\n");
		return;
	}

	fmt->fmt_freq_cnt = dev_pm_opp_get_opp_count(fmt->dev);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(fmt->dev, &freq))) {
		fmt->fmt_freqs[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	i = 0;

	for (i = 0; i < FMT_PORT_NUM; i++)
		fmt->fmt_qos_req[i] = 0;

	i = 0;
	fmt->fmt_qos_req[i++] = of_mtk_icc_get(fmt->dev, "path_mini_mdp_r0");
	fmt->fmt_qos_req[i++] = of_mtk_icc_get(fmt->dev, "path_mini_mdp_r1");
	fmt->fmt_qos_req[i++] = of_mtk_icc_get(fmt->dev, "path_mini_mdp_w0");
	fmt->fmt_qos_req[i++] = of_mtk_icc_get(fmt->dev, "path_mini_mdp_w1");

}

void fmt_unprepare_dvfs_emi_bw(void)
{
}

void fmt_start_dvfs_emi_bw(struct mtk_vdec_fmt *fmt, struct fmt_pmqos pmqos_param, int id)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long request_freq;
	u64 request_freq64;
	struct timespec64 curr_time;
	s32 duration;
	u32 bandwidth;

	fmt_debug(1, "tv_sec %d tv_usec %d pixel_size %d rdma_datasize %d wdma_datasize %d",
			pmqos_param.tv_sec,
			pmqos_param.tv_usec,
			pmqos_param.pixel_size,
			pmqos_param.rdma_datasize,
			pmqos_param.wdma_datasize);

	ktime_get_real_ts64(&curr_time);
	fmt_debug(1, "curr time tv_sec %ld tv_nsec %ld", curr_time.tv_sec, curr_time.tv_nsec);

	FMT_TIMER_GET_DURATION_IN_MS(curr_time, pmqos_param, duration);
	request_freq64 = (u64)pmqos_param.pixel_size * 1000 / duration;
	request_freq = (unsigned long)((request_freq64 > ULONG_MAX) ? ULONG_MAX : request_freq64);

	fmt_debug(1, "request_freq %lu", request_freq);

	if (request_freq > fmt->fmt_freqs[fmt->fmt_freq_cnt-1]) {
		request_freq = fmt->fmt_freqs[fmt->fmt_freq_cnt-1];
		fmt_debug(1, "request_freq %lu limited by highest fmt_freq %lu",
					request_freq, fmt->fmt_freqs[fmt->fmt_freq_cnt-1]);
	}

	if (fmt->fmt_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(fmt->dev,
					&request_freq);
		fmt_debug(1, "actual request freq %lu", request_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(fmt->fmt_reg, volt, INT_MAX);
		if (ret) {
			fmt_debug(0, "Failed to set regulator voltage %d\n",
			volt);
		}
	}
	fmt_debug(1, "rdma cal MMqos (%d, %d, %d)",
			pmqos_param.rdma_datasize,
			pmqos_param.pixel_size,
			request_freq);
	if (id >= 0 && id < fmt->gce_th_num) {
		FMT_BANDWIDTH(pmqos_param.rdma_datasize, pmqos_param.pixel_size,
			request_freq, bandwidth);
		if (fmt->fmt_qos_req[id] != 0) {
			mtk_icc_set_bw(fmt->fmt_qos_req[id],
			MBps_to_icc(bandwidth), 0);
		}
		fmt_debug(1, "rdma bandwidth %d", bandwidth);
		fmt_debug(1, "wdma cal MMqos (%d, %d, %d)",
			pmqos_param.wdma_datasize,
			pmqos_param.pixel_size,
			request_freq);
		FMT_BANDWIDTH(pmqos_param.wdma_datasize, pmqos_param.pixel_size,
			request_freq, bandwidth);
		if (fmt->fmt_qos_req[id+2] != 0) {
			mtk_icc_set_bw(fmt->fmt_qos_req[id+2],
			MBps_to_icc(bandwidth), 0);
		}
		fmt_debug(1, "wdma bandwidth %d", bandwidth);
	}
}

void fmt_end_dvfs_emi_bw(struct mtk_vdec_fmt *fmt, int id)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	if (fmt->fmt_reg != 0) {
		fmt_debug(1, "request freq %lu", fmt->fmt_freqs[0]);
		opp = dev_pm_opp_find_freq_ceil(fmt->dev,
					&fmt->fmt_freqs[0]);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(fmt->fmt_reg, volt, INT_MAX);
		if (ret) {
			fmt_debug(0, "Failed to set regulator voltage %d\n",
			volt);
		}
	}
	if (id >= 0 && id < fmt->gce_th_num) {
		if (fmt->fmt_qos_req[id] != 0) {
			mtk_icc_set_bw(fmt->fmt_qos_req[id],
				MBps_to_icc(0), 0);
		}
		if (fmt->fmt_qos_req[id+2] != 0) {
			mtk_icc_set_bw(fmt->fmt_qos_req[id+2],
				MBps_to_icc(0), 0);
		}
	}
}

