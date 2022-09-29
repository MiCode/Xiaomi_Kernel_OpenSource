// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <linux/clk.h>
//#include <linux/interconnect-provider.h>
#include <linux/interconnect.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_mmdvfs.h>
#include <soc/mediatek/smi.h>
#include <soc/mediatek/dramc.h>
#include "mtk_iommu.h"
#include "mmqos-mtk.h"
#include "mtk_qos_bound.h"

#define CREATE_TRACE_POINTS
#include "mmqos_events.h"

#define SHIFT_ROUND(a, b)	((((a) - 1) >> (b)) + 1)
#define icc_to_MBps(x)		((x) / 1000)
#define MASK_8(a)		((a) & 0xff)
#define MULTIPLY_RATIO(value)	((value)*1000)

#define NODE_TYPE(a)		(a >> 16)
#define LARB_ID(a)		(MASK_8(a))
#define W_BW_RATIO		(8)
#define R_BW_RATIO		(7)

#define MAX_RECORD_COMM_NUM	(2)
#define MAX_RECORD_PORT_NUM	(9)

static u32 mmqos_state = MMQOS_ENABLE;

static int ftrace_ena;

struct comm_port_bw_record {
	u8 idx[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM];
	u64 time[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 larb_id[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 avg_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 peak_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 l_avg_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 l_peak_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
};

struct chn_bw_record {
	u8 idx[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM];
	u64 time[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 srt_r_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 srt_w_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 hrt_r_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 hrt_w_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
};

struct comm_port_bw_record *comm_port_bw_rec;
struct chn_bw_record *chn_bw_rec;

struct common_port_node {
	struct mmqos_base_node *base;
	struct common_node *common;
	struct device *larb_dev;
	struct mutex bw_lock;
	u32 latest_mix_bw;
	u64 latest_peak_bw;
	u32 latest_avg_bw;
	struct list_head list;
	u8 channel;
	u8 hrt_type;
	u32 write_peak_bw;
	u32 write_avg_bw;
};

struct larb_port_node {
	struct mmqos_base_node *base;
	u32 old_avg_bw;
	u32 old_peak_bw;
	u16 bw_ratio;
	u8 channel;
	bool is_max_ostd;
	bool is_write;
};

struct mtk_mmqos {
	struct device *dev;
	struct icc_provider prov;
	struct notifier_block nb;
	struct list_head comm_list;
	//struct workqueue_struct *wq;
	u32 max_ratio;
	bool dual_pipe_enable;
	bool qos_bound; /* Todo: Set qos_bound to true if necessary */

	struct proc_dir_entry *proc;
};

static struct mtk_mmqos *gmmqos;
static struct mmqos_hrt *g_hrt;

static u32 log_level;
enum mmqos_log_level {
	log_bw = 0,
	log_comm_freq,
};

static u32 chn_hrt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 chn_srt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 chn_hrt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 chn_srt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};

static void mmqos_update_comm_bw(struct device *dev,
	u32 comm_port, u32 freq, u64 mix_bw, u64 bw_peak, bool qos_bound, bool max_bwl)
{
	u32 comm_bw = 0;
	u32 value;

	if (!freq || !dev)
		return;
	if (mix_bw)
		comm_bw = (mix_bw << 8) / freq;
	if (max_bwl)
		comm_bw = 0xfff;
	if (comm_bw)
		value = ((comm_bw > 0xfff) ? 0xfff : comm_bw) |
			((bw_peak > 0 || !qos_bound) ? 0x1000 : 0x3000);
	else
		value = 0x1200;
	mtk_smi_common_bw_set(dev, comm_port, value);
	if (log_level & 1 << log_bw)
		dev_notice(dev, "comm port=%d bw=%d freq=%d qos_bound=%d value=%#x\n",
			comm_port, comm_bw, freq, qos_bound, value);
}

static void mmqos_update_comm_ostdl(struct device *dev, u32 comm_port,
		u16 max_ratio, struct icc_node *larb)
{
	struct larb_node *larb_node = (struct larb_node *)larb->data;
	u32 value;
	u16 bw_ratio;

	bw_ratio = larb_node->is_write ? W_BW_RATIO : R_BW_RATIO;
	if (larb->avg_bw) {
		value = SHIFT_ROUND(icc_to_MBps(larb->avg_bw), bw_ratio);
		if (value > max_ratio)
			value = max_ratio;
	} else
		value = 0;

	mtk_smi_common_ostdl_set(dev, comm_port, larb_node->is_write, value);
	if (log_level & 1 << log_bw)
		dev_notice(dev, "%s larb_id=%d comm port=%d is_write=%d bw_ratio=%d avg_bw=%d ostdl=%d\n",
			__func__, LARB_ID(larb->id), comm_port, larb_node->is_write,
			bw_ratio, larb->avg_bw, value);
}

static void mmqos_update_setting(struct mtk_mmqos *mmqos)
{
	struct common_node *comm_node;
	struct common_port_node *comm_port;

	list_for_each_entry(comm_node, &mmqos->comm_list, list) {
		comm_node->freq = clk_get_rate(comm_node->clk)/1000000;
		if (mmqos_state & BWL_ENABLE) {
			list_for_each_entry(comm_port,
						&comm_node->comm_port_list, list) {
				mutex_lock(&comm_port->bw_lock);
				if (comm_port->latest_mix_bw
					|| comm_port->latest_peak_bw) {
					mmqos_update_comm_bw(comm_port->larb_dev,
						MASK_8(comm_port->base->icc_node->id),
						comm_port->common->freq,
						icc_to_MBps(comm_port->latest_mix_bw),
						icc_to_MBps(comm_port->latest_peak_bw),
						mmqos->qos_bound,
						comm_port->hrt_type == HRT_MAX_BWL);
				}
				mutex_unlock(&comm_port->bw_lock);
			}
		}
	}
}


static int update_mm_clk(struct notifier_block *nb,
		unsigned long value, void *v)
{
	struct mtk_mmqos *mmqos =
		container_of(nb, struct mtk_mmqos, nb);

	mmqos_update_setting(mmqos);
	return 0;
}

void mtk_mmqos_is_dualpipe_enable(bool is_enable)
{
	gmmqos->dual_pipe_enable = is_enable;
	pr_notice("%s: %d\n", __func__, gmmqos->dual_pipe_enable);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_is_dualpipe_enable);

s32 mtk_mmqos_system_qos_update(unsigned short qos_status)
{
	struct mtk_mmqos *mmqos = gmmqos;

	if (IS_ERR_OR_NULL(mmqos)) {
		pr_notice("%s is not ready\n", __func__);
		return 0;
	}
	mmqos->qos_bound = (qos_status > QOS_BOUND_BW_FREE);
	mmqos_update_setting(mmqos);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_system_qos_update);

static unsigned long get_volt_by_freq(struct device *dev, unsigned long freq)
{
	struct dev_pm_opp *opp;
	unsigned long ret;

	opp = dev_pm_opp_find_freq_ceil(dev, &freq);

	/* It means freq is over the highest available frequency */
	if (opp == ERR_PTR(-ERANGE))
		opp = dev_pm_opp_find_freq_floor(dev, &freq);

	if (IS_ERR(opp)) {
		dev_notice(dev, "%s failed(%d) freq=%lu\n",
			__func__, PTR_ERR(opp), freq);
		return 0;
	}

	ret = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	return ret;
}

//static void set_comm_icc_bw_handler(struct work_struct *work)
static void set_comm_icc_bw(struct common_node *comm_node)
{
	struct common_port_node *comm_port_node;
	u32 avg_bw = 0, peak_bw = 0, max_bw = 0;
	u64 normalize_peak_bw;
	unsigned long smi_clk = 0;
	u32 volt, i, j, comm_id;
	s32 ret;

	MMQOS_SYSTRACE_BEGIN("%s %s\n", __func__, comm_node->base->icc_node->name);
	list_for_each_entry(comm_port_node, &comm_node->comm_port_list, list) {
		mutex_lock(&comm_port_node->bw_lock);
		avg_bw += comm_port_node->latest_avg_bw;
		if (comm_port_node->hrt_type < HRT_TYPE_NUM) {
			normalize_peak_bw = MULTIPLY_RATIO(comm_port_node->latest_peak_bw)
						/ mtk_mmqos_get_hrt_ratio(
						comm_port_node->hrt_type);
			peak_bw += normalize_peak_bw;
		}
		mutex_unlock(&comm_port_node->bw_lock);
	}

	comm_id = MASK_8(comm_node->base->icc_node->id);
	for (i = 0; i < MMQOS_COMM_CHANNEL_NUM; i++) {
		max_bw = max_t(u32, max_bw, chn_hrt_r_bw[comm_id][i] * 10 / 7);
		max_bw = max_t(u32, max_bw, chn_srt_r_bw[comm_id][i]);
		max_bw = max_t(u32, max_bw, chn_hrt_w_bw[comm_id][i] * 10 / 7);
		max_bw = max_t(u32, max_bw, chn_srt_w_bw[comm_id][i]);
	}

	if (max_bw)
		smi_clk = SHIFT_ROUND(max_bw, 4) * 1000;
	else
		smi_clk = 0;


	if (comm_node->comm_dev && smi_clk != comm_node->smi_clk) {
		volt = get_volt_by_freq(comm_node->comm_dev, smi_clk);
		if (volt > 0 && volt != comm_node->volt) {
			if (log_level & 1 << log_comm_freq) {
				for (i = 0; i < MMQOS_MAX_COMM_NUM; i++) {
					for (j = 0; j < MMQOS_COMM_CHANNEL_NUM; j++) {
						dev_notice(comm_node->comm_dev,
						"comm(%d) chn=%d s_r=%u h_r=%u s_w=%u h_w=%u\n",
						i, j, chn_srt_r_bw[i][j], chn_hrt_r_bw[i][j],
						chn_srt_w_bw[i][j], chn_hrt_w_bw[i][j]);
					}
				}
				dev_notice(comm_node->comm_dev,
					"comm(%d) max_bw=%u smi_clk=%u volt=%u\n",
					comm_id, max_bw, smi_clk, volt);
			}
			if (IS_ERR_OR_NULL(comm_node->comm_reg)) {
				if (IS_ERR_OR_NULL(comm_node->clk))
					dev_notice(comm_node->comm_dev,
						"mmdvfs clk is not ready\n");
				else {
					ret = clk_set_rate(comm_node->clk, smi_clk);
					if (ret)
						dev_notice(comm_node->comm_dev,
							"clk_set_rate failed:%d\n", ret);
				}
			} else if (regulator_set_voltage(comm_node->comm_reg,
					volt, INT_MAX))
				dev_notice(comm_node->comm_dev,
					"regulator_set_voltage failed volt=%lu\n", volt);
			comm_node->volt = volt;

		}
		comm_node->smi_clk = smi_clk;
	}
	MMQOS_SYSTRACE_BEGIN("to EMI avg %d peak %d\n", avg_bw, peak_bw);
	icc_set_bw(comm_node->icc_path, avg_bw, 0);
	icc_set_bw(comm_node->icc_hrt_path, peak_bw, 0);
	MMQOS_SYSTRACE_END();

	MMQOS_SYSTRACE_END();
}

static void update_hrt_bw(struct mtk_mmqos *mmqos)
{
	struct common_node *comm_node;
	struct common_port_node *comm_port;
	u32 hrt_bw[HRT_TYPE_NUM] = {0};
	u32 i;

	list_for_each_entry(comm_node, &mmqos->comm_list, list) {
		list_for_each_entry(comm_port,
				    &comm_node->comm_port_list, list) {
			if (comm_port->hrt_type < HRT_TYPE_NUM) {
				mutex_lock(&comm_port->bw_lock);
				hrt_bw[comm_port->hrt_type] +=
					icc_to_MBps(comm_port->latest_peak_bw);
				mutex_unlock(&comm_port->bw_lock);
			}
		}
	}

	for (i = 0; i < HRT_TYPE_NUM; i++)
		if (i != HRT_MD)
			mtk_mmqos_set_hrt_bw(i, hrt_bw[i]);

}

static void record_comm_port_bw(u32 comm_id, u32 port_id, u32 larb_id,
	u32 avg_bw, u32 peak_bw, u32 l_avg, u32 l_peak)
{
	u32 idx;

	idx = comm_port_bw_rec->idx[comm_id][port_id];
	comm_port_bw_rec->time[comm_id][port_id][idx] = sched_clock();
	comm_port_bw_rec->larb_id[comm_id][port_id][idx] = larb_id;
	comm_port_bw_rec->avg_bw[comm_id][port_id][idx] = avg_bw;
	comm_port_bw_rec->peak_bw[comm_id][port_id][idx] = peak_bw;
	comm_port_bw_rec->l_avg_bw[comm_id][port_id][idx] = l_avg;
	comm_port_bw_rec->l_peak_bw[comm_id][port_id][idx] = l_peak;
	comm_port_bw_rec->idx[comm_id][port_id] = (idx + 1) % RECORD_NUM;
}

static void record_chn_bw(u32 comm_id, u32 chnn_id, u32 srt_r, u32 srt_w, u32 hrt_r, u32 hrt_w)
{
	u32 idx;

	idx = chn_bw_rec->idx[comm_id][chnn_id];
	chn_bw_rec->time[comm_id][chnn_id][idx] = sched_clock();
	chn_bw_rec->srt_r_bw[comm_id][chnn_id][idx] = srt_r;
	chn_bw_rec->srt_w_bw[comm_id][chnn_id][idx] = srt_w;
	chn_bw_rec->hrt_r_bw[comm_id][chnn_id][idx] = hrt_r;
	chn_bw_rec->hrt_w_bw[comm_id][chnn_id][idx] = hrt_w;
	chn_bw_rec->idx[comm_id][chnn_id] = (idx + 1) % RECORD_NUM;
}

static int mtk_mmqos_set(struct icc_node *src, struct icc_node *dst)
{
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct common_port_node *comm_port_node;
	struct common_node *comm_node;
	struct mtk_mmqos *mmqos = container_of(dst->provider,
					struct mtk_mmqos, prov);
	u32 value = 1;
	u32 comm_id, chnn_id, port_id;

	MMQOS_SYSTRACE_BEGIN("%s %s->%s\n", __func__, src->name, dst->name);
	switch (NODE_TYPE(dst->id)) {
	case MTK_MMQOS_NODE_COMMON:
		comm_node = (struct common_node *)dst->data;
		if (!comm_node)
			break;
		if (mmqos_state & DVFSRC_ENABLE) {
			set_comm_icc_bw(comm_node);
			update_hrt_bw(mmqos);
		}
		//queue_work(mmqos->wq, &comm_node->work);
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		comm_port_node = (struct common_port_node *)dst->data;
		larb_node = (struct larb_node *)src->data;
		if (!comm_port_node || !larb_node)
			break;
		comm_id = (larb_node->channel >> 4) & 0xf;
		chnn_id = larb_node->channel & 0xf;
		if (chnn_id) {
			chnn_id -= 1;
			if (larb_node->is_write) {
				chn_hrt_w_bw[comm_id][chnn_id] -= larb_node->old_peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] -= larb_node->old_avg_bw;
				chn_hrt_w_bw[comm_id][chnn_id] += src->peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] += src->avg_bw;
				larb_node->old_peak_bw = src->peak_bw;
				larb_node->old_avg_bw = src->avg_bw;
			} else {
				if (comm_port_node->hrt_type == HRT_DISP
					&& gmmqos->dual_pipe_enable) {
					chn_hrt_r_bw[comm_id][chnn_id] -= larb_node->old_peak_bw;
					chn_hrt_r_bw[comm_id][chnn_id] += (src->peak_bw / 2);
					larb_node->old_peak_bw = (src->peak_bw / 2);
				} else {
					chn_hrt_r_bw[comm_id][chnn_id] -= larb_node->old_peak_bw;
					chn_hrt_r_bw[comm_id][chnn_id] += src->peak_bw;
					larb_node->old_peak_bw = src->peak_bw;
				}

				chn_srt_r_bw[comm_id][chnn_id] -= larb_node->old_avg_bw;
				chn_srt_r_bw[comm_id][chnn_id] += src->avg_bw;
				larb_node->old_avg_bw = src->avg_bw;

				if (log_level & 1 << log_bw)
					pr_notice("comm=%d chnn=%d larb=%d avg_bw:%d peak_bw:%d s_r:%d h_r:%d\n",
						comm_id, chnn_id, LARB_ID(src->id),
						icc_to_MBps(src->avg_bw),
						icc_to_MBps(src->peak_bw),
						chn_srt_r_bw[comm_id][chnn_id],
						chn_hrt_r_bw[comm_id][chnn_id]);
			}

			if (mmqos_met_enabled()) {
				trace_mmqos__larb_avg_bw(
					LARB_ID(src->id),
					icc_to_MBps(src->avg_bw));
				trace_mmqos__larb_peak_bw(
					LARB_ID(src->id),
					icc_to_MBps(src->peak_bw));
				trace_mmqos__chn_bw(comm_id, chnn_id,
					icc_to_MBps(chn_srt_r_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_srt_w_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_hrt_r_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_hrt_w_bw[comm_id][chnn_id]));
			}
		}
		mutex_lock(&comm_port_node->bw_lock);
		if (comm_port_node->latest_mix_bw == comm_port_node->base->mix_bw
			&& comm_port_node->latest_peak_bw == dst->peak_bw
			&& comm_port_node->latest_avg_bw == dst->avg_bw) {
			mutex_unlock(&comm_port_node->bw_lock);
			break;
		}
		comm_port_node->latest_mix_bw = comm_port_node->base->mix_bw;
		comm_port_node->latest_peak_bw = dst->peak_bw;
		comm_port_node->latest_avg_bw = dst->avg_bw;
		port_id = MASK_8(dst->id);
		if (mmqos_state & BWL_ENABLE)
			mmqos_update_comm_bw(comm_port_node->larb_dev,
				port_id, comm_port_node->common->freq,
				icc_to_MBps(comm_port_node->latest_mix_bw),
				icc_to_MBps(comm_port_node->latest_peak_bw),
				mmqos->qos_bound, comm_port_node->hrt_type == HRT_MAX_BWL);

		if ((mmqos_state & P2_COMM_OSTDL_ENABLE)
			&& larb_node->is_p2_larb)
			mmqos_update_comm_ostdl(comm_port_node->larb_dev,
				port_id, mmqos->max_ratio, src);

		record_comm_port_bw(comm_id, port_id, LARB_ID(src->id),
			src->avg_bw, src->peak_bw,
			comm_port_node->latest_avg_bw,
			comm_port_node->latest_peak_bw);
		record_chn_bw(comm_id, chnn_id,
			chn_srt_r_bw[comm_id][chnn_id],
			chn_srt_w_bw[comm_id][chnn_id],
			chn_hrt_r_bw[comm_id][chnn_id],
			chn_hrt_w_bw[comm_id][chnn_id]);

		mutex_unlock(&comm_port_node->bw_lock);
		break;
	case MTK_MMQOS_NODE_LARB:
		larb_port_node = (struct larb_port_node *)src->data;
		larb_node = (struct larb_node *)dst->data;
		if (!larb_port_node || !larb_node || !larb_node->larb_dev)
			break;
		/* update channel BW */
		comm_id = (larb_port_node->channel >> 4) & 0xf;
		chnn_id = larb_port_node->channel & 0xf;
		if (chnn_id) {
			chnn_id -= 1;
			if (larb_port_node->is_write) {
				chn_hrt_w_bw[comm_id][chnn_id] -= larb_port_node->old_peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] -= larb_port_node->old_avg_bw;
				chn_hrt_w_bw[comm_id][chnn_id] += src->peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] += src->avg_bw;
				larb_port_node->old_peak_bw = src->peak_bw;
				larb_port_node->old_avg_bw = src->avg_bw;
			} else {
				chn_hrt_r_bw[comm_id][chnn_id] -= larb_port_node->old_peak_bw;
				chn_srt_r_bw[comm_id][chnn_id] -= larb_port_node->old_avg_bw;
				chn_hrt_r_bw[comm_id][chnn_id] += src->peak_bw;
				chn_srt_r_bw[comm_id][chnn_id] += src->avg_bw;
				larb_port_node->old_peak_bw = src->peak_bw;
				larb_port_node->old_avg_bw = src->avg_bw;
			}
		}

		if (larb_port_node->base->mix_bw) {
			value = SHIFT_ROUND(
				icc_to_MBps(larb_port_node->base->mix_bw),
				larb_port_node->bw_ratio);
			if (src->peak_bw)
				value = SHIFT_ROUND(value * 3, 1);
		} else {
			larb_port_node->is_max_ostd = false;
		}
		if (value > mmqos->max_ratio || larb_port_node->is_max_ostd)
			value = mmqos->max_ratio;
		if (mmqos_state & OSTD_ENABLE)
			mtk_smi_larb_bw_set(
				larb_node->larb_dev,
				MTK_M4U_TO_PORT(src->id), value);

		if (log_level & 1 << log_bw)
			dev_notice(larb_node->larb_dev,
				"larb=%d port=%d avg_bw:%d peak_bw:%d ostd=%#x\n",
				MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
				icc_to_MBps(larb_port_node->base->icc_node->avg_bw),
				icc_to_MBps(larb_port_node->base->icc_node->peak_bw),
				value);

		if (mmqos_met_enabled()) {
			trace_mmqos__larb_port_avg_bw(
				MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
				icc_to_MBps(larb_port_node->base->icc_node->avg_bw));
			trace_mmqos__larb_port_peak_bw(
				MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
				icc_to_MBps(larb_port_node->base->icc_node->peak_bw));
			trace_mmqos__chn_bw(comm_id, chnn_id,
				icc_to_MBps(chn_srt_r_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_srt_w_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_hrt_r_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_hrt_w_bw[comm_id][chnn_id]));
		}
		//queue_work(mmqos->wq, &larb_node->work);
		break;
	default:
		break;
	}
	MMQOS_SYSTRACE_END();
	return 0;
}

static int mtk_mmqos_aggregate(struct icc_node *node,
	u32 tag, u32 avg_bw, u32 peak_bw, u32 *agg_avg,
	u32 *agg_peak)
{
	struct mmqos_base_node *base_node = NULL;
	struct larb_port_node *larb_port_node;
	u32 mix_bw = peak_bw;

	if (!node || !node->data)
		return 0;

	MMQOS_SYSTRACE_BEGIN("%s %s\n", __func__, node->name);
	switch (NODE_TYPE(node->id)) {
	case MTK_MMQOS_NODE_LARB_PORT:
		larb_port_node = (struct larb_port_node *)node->data;
		base_node = larb_port_node->base;
		if (peak_bw) {
			if (peak_bw == MTK_MMQOS_MAX_BW) {
				larb_port_node->is_max_ostd = true;
				mix_bw = max_t(u32, avg_bw, 1000);
			} else {
				mix_bw = peak_bw;
			}
		}
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		base_node = ((struct common_port_node *)node->data)->base;
		break;
	//default:
	//	return 0;
	}
	if (base_node) {
		if (*agg_avg == 0 && *agg_peak == 0)
			base_node->mix_bw = 0;
		base_node->mix_bw += peak_bw ? mix_bw : avg_bw;
	}
	*agg_avg += avg_bw;

	if (peak_bw == MTK_MMQOS_MAX_BW)
		*agg_peak += 1000; /* for BWL soft mode */
	else
		*agg_peak += peak_bw;

	MMQOS_SYSTRACE_END();
	return 0;
}

static struct icc_node *mtk_mmqos_xlate(
	struct of_phandle_args *spec, void *data)
{
	struct icc_onecell_data *icc_data;
	s32 i;

	if (!spec || !data)
		return ERR_PTR(-EPROBE_DEFER);
	icc_data = (struct icc_onecell_data *)data;
	for (i = 0; i < icc_data->num_nodes; i++)
		if (icc_data->nodes[i]->id == spec->args[0])
			return icc_data->nodes[i];
	pr_notice("%s: invalid index %u\n", __func__, spec->args[0]);
	return ERR_PTR(-EINVAL);
}

static void comm_port_bw_dump(struct seq_file *file, u32 comm_id, u32 port_id, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = comm_port_bw_rec->time[comm_id][port_id][i];
	rem_nsec = do_div(ts, 1000000000);
	if (ts == 0 &&
		comm_port_bw_rec->avg_bw[comm_id][port_id][i] == 0 &&
		comm_port_bw_rec->peak_bw[comm_id][port_id][i] == 0 &&
		comm_port_bw_rec->l_avg_bw[comm_id][port_id][i] == 0 &&
		comm_port_bw_rec->l_peak_bw[comm_id][port_id][i] == 0)
		return;

	seq_printf(file, "[%5lu.%06lu] comm%d port%d larb%2d %8d %8d %8d %8d\n",
		(u64)ts, rem_nsec / 1000,
		comm_id, port_id,
		comm_port_bw_rec->larb_id[comm_id][port_id][i],
		icc_to_MBps(comm_port_bw_rec->avg_bw[comm_id][port_id][i]),
		icc_to_MBps(comm_port_bw_rec->peak_bw[comm_id][port_id][i]),
		icc_to_MBps(comm_port_bw_rec->l_avg_bw[comm_id][port_id][i]),
		icc_to_MBps(comm_port_bw_rec->l_peak_bw[comm_id][port_id][i]));
}

static void chn_bw_dump(struct seq_file *file, u32 comm_id, u32 chnn_id, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = chn_bw_rec->time[comm_id][chnn_id][i];
	rem_nsec = do_div(ts, 1000000000);
	seq_printf(file, "[%5lu.%06lu] comm%d_%d %8d %8d %8d %8d\n",
		(u64)ts, rem_nsec / 1000,
		comm_id, chnn_id,
		icc_to_MBps(chn_bw_rec->srt_r_bw[comm_id][chnn_id][i]),
		icc_to_MBps(chn_bw_rec->srt_w_bw[comm_id][chnn_id][i]),
		icc_to_MBps(chn_bw_rec->hrt_r_bw[comm_id][chnn_id][i]),
		icc_to_MBps(chn_bw_rec->hrt_w_bw[comm_id][chnn_id][i]));
}

static void hrt_bw_dump(struct seq_file *file, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = g_hrt->hrt_rec.time[i];
	rem_nsec = do_div(ts, 1000000000);
	seq_printf(file, "[%5lu.%06lu]     %8d %8d %8d\n",
		(u64)ts, rem_nsec / 1000,
		g_hrt->hrt_rec.avail_hrt[i],
		g_hrt->hrt_rec.cam_max_hrt[i],
		g_hrt->hrt_rec.cam_hrt[i]);
}

static void comm_port_bw_full_dump(struct seq_file *file, u32 comm_id, u32 port_id)
{
	u32 i, start;

	start = comm_port_bw_rec->idx[comm_id][port_id];
	for (i = start; i < RECORD_NUM; i++)
		comm_port_bw_dump(file, comm_id, port_id, i);

	for (i = 0; i < start; i++)
		comm_port_bw_dump(file, comm_id, port_id, i);

}

static void chn_bw_full_dump(struct seq_file *file, u32 comm_id, u32 chnn_id)
{
	u32 i, start;

	start = chn_bw_rec->idx[comm_id][chnn_id];
	for (i = start; i < RECORD_NUM; i++)
		chn_bw_dump(file, comm_id, chnn_id, i);

	for (i = 0; i < start; i++)
		chn_bw_dump(file, comm_id, chnn_id, i);

}

static void hrt_bw_full_dump(struct seq_file *file)
{
	u32 i, start;
	struct hrt_record *rec = &g_hrt->hrt_rec;

	start = rec->idx;
	for (i = start; i < RECORD_NUM; i++)
		hrt_bw_dump(file, i);

	for (i = 0; i < start; i++)
		hrt_bw_dump(file, i);

}

static int mmqos_bw_dump(struct seq_file *file, void *data)
{
	u32 comm_id = 0, chnn_id = 0, port_id = 0;

	seq_printf(file, "MMQoS HRT BW Dump: %8s %8s %8s\n",
		"avail", "cam_max", "cam_hrt");
	hrt_bw_full_dump(file);

	seq_printf(file, "MMQoS Channel BW Dump: %8s %8s %8s %8s\n",
		"s_r", "s_w", "h_r", "h_w");
	for (comm_id = 0; comm_id < MAX_RECORD_COMM_NUM; comm_id++) {
		for (chnn_id = 0; chnn_id < MMQOS_COMM_CHANNEL_NUM; chnn_id++)
			chn_bw_full_dump(file, comm_id, chnn_id);
	}

	seq_printf(file, "MMQoS Common Port BW Dump:        %8s %8s %8s %8s\n",
		"avg", "peak", "l_avg", "l_peak");
	for (comm_id = 0; comm_id < MAX_RECORD_COMM_NUM; comm_id++) {
		for (port_id = 0; port_id < MAX_RECORD_PORT_NUM; port_id++)
			comm_port_bw_full_dump(file, comm_id, port_id);
	}
	return 0;
}

static int mmqos_debug_opp_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmqos_bw_dump, inode->i_private);
}

static const struct proc_ops mmqos_debug_fops = {
	.proc_open = mmqos_debug_opp_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int mtk_mmqos_probe(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos;
	struct of_phandle_iterator it;
	struct icc_onecell_data *data;
	struct icc_node *node, *temp;
	struct mmqos_base_node *base_node;
	struct common_node *comm_node;
	struct common_port_node *comm_port_node;
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct mtk_iommu_data *smi_imu;
	int i, j, id, num_larbs = 0, ret, ddr_type;
	const struct mtk_mmqos_desc *mmqos_desc;
	const struct mtk_node_desc *node_desc;
	struct device *larb_dev;
	struct mmqos_hrt *hrt;
	struct device_node *np;
	struct platform_device *comm_pdev, *larb_pdev;
	struct proc_dir_entry *dir, *proc;

	mmqos = devm_kzalloc(&pdev->dev, sizeof(*mmqos), GFP_KERNEL);
	if (!mmqos)
		return -ENOMEM;
	gmmqos = mmqos;

	mmqos->dev = &pdev->dev;
	smi_imu = devm_kzalloc(&pdev->dev, sizeof(*smi_imu), GFP_KERNEL);
	if (!smi_imu)
		return -ENOMEM;

	chn_bw_rec = devm_kzalloc(&pdev->dev,
		sizeof(*chn_bw_rec), GFP_KERNEL);
	if (!chn_bw_rec)
		return -ENOMEM;

	comm_port_bw_rec = devm_kzalloc(&pdev->dev,
		sizeof(*comm_port_bw_rec), GFP_KERNEL);
	if (!comm_port_bw_rec)
		return -ENOMEM;

	of_for_each_phandle(
		&it, ret, pdev->dev.of_node, "mediatek,larbs", NULL, 0) {
		np = of_node_get(it.node);
		if (!of_device_is_available(np))
			continue;
		larb_pdev = of_find_device_by_node(np);
		if (!larb_pdev) {
			larb_pdev = of_platform_device_create(
				np, NULL, platform_bus_type.dev_root);
			if (!larb_pdev || !larb_pdev->dev.driver) {
				of_node_put(np);
				return -EPROBE_DEFER;
			}
		}
		if (of_property_read_u32(np, "mediatek,larb-id", &id))
			id = num_larbs;
		smi_imu->larb_imu[id].dev = &larb_pdev->dev;
		num_larbs += 1;
	}
	INIT_LIST_HEAD(&mmqos->comm_list);
	INIT_LIST_HEAD(&mmqos->prov.nodes);
	mmqos->prov.set = mtk_mmqos_set;
	mmqos->prov.aggregate = mtk_mmqos_aggregate;
	mmqos->prov.xlate = mtk_mmqos_xlate;
	mmqos->prov.dev = &pdev->dev;
	ret = mtk_icc_provider_add(&mmqos->prov);
	if (ret) {
		dev_notice(&pdev->dev, "mtk_icc_provider_add failed:%d\n", ret);
		return ret;
	}
	mmqos_desc = (struct mtk_mmqos_desc *)
		of_device_get_match_data(&pdev->dev);
	if (!mmqos_desc) {
		ret = -EINVAL;
		goto err;
	}
	data = devm_kzalloc(&pdev->dev,
		sizeof(*data) + mmqos_desc->num_nodes * sizeof(node),
		GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	for (i = 0; i < mmqos_desc->num_nodes; i++) {
		node_desc = &mmqos_desc->nodes[i];
		node = mtk_icc_node_create(node_desc->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}
		mtk_icc_node_add(node, &mmqos->prov);
		if (node_desc->link != MMQOS_NO_LINK) {
			ret = mtk_icc_link_create(node, node_desc->link);
			if (ret)
				goto err;
		}
		node->name = node_desc->name;
		base_node = devm_kzalloc(
			&pdev->dev, sizeof(*base_node), GFP_KERNEL);
		if (!base_node) {
			ret = -ENOMEM;
			goto err;
		}
		base_node->icc_node = node;
		switch (NODE_TYPE(node->id)) {
		case MTK_MMQOS_NODE_COMMON:
			comm_node = devm_kzalloc(
				&pdev->dev, sizeof(*comm_node), GFP_KERNEL);
			if (!comm_node) {
				ret = -ENOMEM;
				goto err;
			}
			//INIT_WORK(&comm_node->work, set_comm_icc_bw_handler);
			comm_node->clk = devm_clk_get(&pdev->dev,
				mmqos_desc->comm_muxes[MASK_8(node->id)]);
			if (IS_ERR(comm_node->clk)) {
				dev_notice(&pdev->dev, "get clk fail:%s\n",
					mmqos_desc->comm_muxes[
						MASK_8(node->id)]);
				ret = -EINVAL;
				goto err;
			}

			comm_node->freq = clk_get_rate(comm_node->clk)/1000000;
			INIT_LIST_HEAD(&comm_node->list);
			list_add_tail(&comm_node->list, &mmqos->comm_list);
			INIT_LIST_HEAD(&comm_node->comm_port_list);
			comm_node->icc_path = of_icc_get(&pdev->dev,
				mmqos_desc->comm_icc_path_names[
						MASK_8(node->id)]);
			if (IS_ERR_OR_NULL(comm_node->icc_path)) {
				dev_notice(&pdev->dev,
					"get icc_path fail:%s\n",
					mmqos_desc->comm_icc_path_names[
						MASK_8(node->id)]);
				ret = -EINVAL;
				goto err;
			}
			comm_node->icc_hrt_path = of_icc_get(&pdev->dev,
				mmqos_desc->comm_icc_hrt_path_names[
						MASK_8(node->id)]);
			if (IS_ERR_OR_NULL(comm_node->icc_hrt_path)) {
				dev_notice(&pdev->dev,
					"get icc_hrt_path fail:%s\n",
					mmqos_desc->comm_icc_hrt_path_names[
						MASK_8(node->id)]);
				ret = -EINVAL;
				goto err;
			}
			np = of_parse_phandle(pdev->dev.of_node,
					      "mediatek,commons",
					      MASK_8(node->id));
			if (!of_device_is_available(np)) {
				pr_notice("get common(%d) dev fail\n",
					  MASK_8(node->id));
				break;
			}
			comm_pdev = of_find_device_by_node(np);
			if (comm_pdev)
				comm_node->comm_dev = &comm_pdev->dev;
			else
				pr_notice("comm(%d) pdev is null\n",
					  MASK_8(node->id));
			comm_node->comm_reg =
				devm_regulator_get_optional(comm_node->comm_dev,
						   "mmdvfs-dvfsrc-vcore");
			if (IS_ERR_OR_NULL(comm_node->comm_reg))
				pr_notice("get common(%d) reg fail\n",
				  MASK_8(node->id));

			dev_pm_opp_of_add_table(comm_node->comm_dev);
			comm_node->base = base_node;
			node->data = (void *)comm_node;
			break;
		case MTK_MMQOS_NODE_COMMON_PORT:
			comm_port_node = devm_kzalloc(&pdev->dev,
				sizeof(*comm_port_node), GFP_KERNEL);
			if (!comm_port_node) {
				ret = -ENOMEM;
				goto err;
			}
			comm_port_node->channel =
				mmqos_desc->comm_port_channels[
				MASK_8((node->id >> 8))][MASK_8(node->id)];
			comm_port_node->hrt_type =
				mmqos_desc->comm_port_hrt_types[
				MASK_8((node->id >> 8))][MASK_8(node->id)];
			mutex_init(&comm_port_node->bw_lock);
			comm_port_node->common = node->links[0]->data;
			INIT_LIST_HEAD(&comm_port_node->list);
			list_add_tail(&comm_port_node->list,
				      &comm_port_node->common->comm_port_list);
			comm_port_node->base = base_node;
			node->data = (void *)comm_port_node;
			break;
		case MTK_MMQOS_NODE_LARB:
			larb_node = devm_kzalloc(
				&pdev->dev, sizeof(*larb_node), GFP_KERNEL);
			if (!larb_node) {
				ret = -ENOMEM;
				goto err;
			}
			comm_port_node = node->links[0]->data;
			larb_dev = smi_imu->larb_imu[node->id &
					(MTK_LARB_NR_MAX-1)].dev;
			if (larb_dev) {
				comm_port_node->larb_dev = larb_dev;
				larb_node->larb_dev = larb_dev;
			}
			//INIT_WORK(&larb_node->work, set_larb_icc_bw_handler);

			larb_node->channel = node_desc->channel;
			larb_node->is_write = node_desc->is_write;
			/* init disable dualpipe */
			gmmqos->dual_pipe_enable = false;

			for (j = 0; j < MMQOS_MAX_P2_LARB_NUM; j++) {
				if (node->id == mmqos_desc->p2_larbs[j])
					larb_node->is_p2_larb = true;
			}
			larb_node->base = base_node;
			node->data = (void *)larb_node;
			break;
		case MTK_MMQOS_NODE_LARB_PORT:
			larb_port_node = devm_kzalloc(&pdev->dev,
				sizeof(*larb_port_node), GFP_KERNEL);
			if (!larb_port_node) {
				ret = -ENOMEM;
				goto err;
			}
			larb_port_node->channel = node_desc->channel;
			larb_port_node->is_write = node_desc->is_write;
			larb_port_node->bw_ratio = node_desc->bw_ratio;
			larb_port_node->base = base_node;
			node->data = (void *)larb_port_node;
			break;
		default:
			dev_notice(&pdev->dev,
				"invalid node id:%#x\n", node->id);
			ret = -EINVAL;
			goto err;
		}
		data->nodes[i] = node;
	}
	data->num_nodes = mmqos_desc->num_nodes;
	mmqos->prov.data = data;


	mmqos->max_ratio = mmqos_desc->max_ratio;

	mmqos_state = mmqos_desc->mmqos_state ?
			mmqos_desc->mmqos_state : mmqos_state;
	pr_notice("[mmqos] mmqos probe state: %d", mmqos_state);
	if (of_property_read_bool(pdev->dev.of_node, "disable-mmqos")) {
		mmqos_state = MMQOS_DISABLE;
		pr_notice("[mmqos] mmqos init disable: %d", mmqos_state);
	}

	/*
	mmqos->wq = create_singlethread_workqueue("mmqos_work_queue");
	if (!mmqos->wq) {
		dev_notice(&pdev->dev, "work queue create fail\n");
		ret = -ENOMEM;
		goto err;
	}
	*/
	hrt = devm_kzalloc(&pdev->dev, sizeof(*hrt), GFP_KERNEL);
	if (!hrt) {
		ret = -ENOMEM;
		goto err;
	}

	ddr_type = mtk_dramc_get_ddr_type();
	if (ddr_type == TYPE_LPDDR4 ||
		ddr_type == TYPE_LPDDR4X || ddr_type == TYPE_LPDDR4P)
		memcpy(hrt, &mmqos_desc->hrt_LPDDR4, sizeof(mmqos_desc->hrt_LPDDR4));
	else
		memcpy(hrt, &mmqos_desc->hrt, sizeof(mmqos_desc->hrt));
	pr_notice("[mmqos] ddr type: %d\n", mtk_dramc_get_ddr_type());

	hrt->md_scen = mmqos_desc->md_scen;
	mtk_mmqos_init_hrt(hrt);
	g_hrt = hrt;
	mmqos->nb.notifier_call = update_mm_clk;
	register_mmdvfs_notifier(&mmqos->nb);
	ret = mtk_mmqos_register_hrt_sysfs(&pdev->dev);
	if (ret)
		dev_notice(&pdev->dev, "sysfs create fail\n");
	platform_set_drvdata(pdev, mmqos);
	devm_kfree(&pdev->dev, smi_imu);

	/* create proc file */
	dir = proc_mkdir("mmqos", NULL);
	if (IS_ERR_OR_NULL(dir))
		pr_notice("proc_mkdir failed:%ld\n", PTR_ERR(dir));

	proc = proc_create("mmqos_bw", 0444, dir, &mmqos_debug_fops);
	if (IS_ERR_OR_NULL(proc))
		pr_notice("proc_create failed:%ld\n", PTR_ERR(proc));
	else
		mmqos->proc = proc;

	return 0;
err:
	list_for_each_entry_safe(node, temp, &mmqos->prov.nodes, node_list) {
		mtk_icc_node_del(node);
		mtk_icc_node_destroy(node->id);
	}
	mtk_icc_provider_del(&mmqos->prov);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_probe);

int mtk_mmqos_remove(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos = platform_get_drvdata(pdev);
	struct icc_node *node, *temp;

	list_for_each_entry_safe(node, temp, &mmqos->prov.nodes, node_list) {
		mtk_icc_node_del(node);
		mtk_icc_node_destroy(node->id);
	}
	mtk_icc_provider_del(&mmqos->prov);
	unregister_mmdvfs_notifier(&mmqos->nb);
	//destroy_workqueue(mmqos->wq);
	mtk_mmqos_unregister_hrt_sysfs(&pdev->dev);
	return 0;
}

bool mmqos_met_enabled(void)
{
	return ftrace_ena & (1 << MMQOS_PROFILE_MET);
}

bool mmqos_systrace_enabled(void)
{
	return ftrace_ena & (1 << MMQOS_PROFILE_SYSTRACE);
}

noinline int tracing_mark_write(char *fmt, ...)
{
#if IS_ENABLED(CONFIG_MTK_FTRACER)
	char buf[TRACE_MSG_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= TRACE_MSG_LEN) {
		pr_notice("%s trace size %u exceed limit\n", __func__, len);
		return -1;
	}

	trace_puts(buf);
#endif
	return 0;
}

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmqos log level");

module_param(mmqos_state, uint, 0644);
MODULE_PARM_DESC(mmqos_state, "mmqos_state");

module_param(ftrace_ena, uint, 0644);
MODULE_PARM_DESC(ftrace_ena, "ftrace enable");

EXPORT_SYMBOL_GPL(mtk_mmqos_remove);
MODULE_LICENSE("GPL v2");
