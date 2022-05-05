// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/devfreq.h>

#include "apusys_power.h"
#include "apupw_tag.h"
#include "apu_dbg.h"
#include "apu_regulator.h"
#include "apu_clk.h"
#include "apu_trace.h"
#include "apu_rpc.h"

#define CREATE_TRACE_POINTS
#include "events/apu_power_events.h"
#include "events/apu_events.h"

static struct apupwr_tag pw_tag = {
	.rpc = { .vpu0_cg_stat = 0xdb,
			 .vpu1_cg_stat = 0xdb,
			 .vpu2_cg_stat = 0xdb,
			 .mdla0_cg_stat = 0xdb,
			 .mdla1_cg_stat = 0xdb,
		   },
};

void apupw_dbg_pwr_tag_update(struct apu_dev *ad, ulong rate, ulong volt)
{
	struct apupwr_tag_pwr *pwr = &pw_tag.pwr;

	switch (ad->user) {
	case APUCORE:
		pwr->vcore = volt;
		pwr->ipuif_freq = rate;
		break;
	case APUCONN:
		pwr->vvpu = volt;
		pwr->vsram = ad->argul->rgul_sup->cur_volt;
		pwr->dsp_freq = rate;
		break;
	case APUIOMMU:
		pwr->dsp7_freq = rate;
		break;
	case MDLA: /* mdla in 6885/6873/6853/6889 are share same pll */
		pwr->vmdla = volt;
		pwr->dsp5_freq = rate;
		if (!IS_ERR(apu_find_device(MDLA1)))
			pwr->dsp6_freq = rate;
		break;
	case VPU0:
		if (ad->df->profile->target != devfreq_dummy_target)
			pwr->dsp1_freq = rate;
		break;
	case VPU1:
		if (ad->df->profile->target != devfreq_dummy_target)
			pwr->dsp2_freq = rate;
		break;
	case VPU2:
		if (ad->df->profile->target != devfreq_dummy_target)
			pwr->dsp3_freq = rate;
		break;
	case VPU:
		if (ad->df->profile->target != devfreq_dummy_target) {
			pwr->dsp1_freq = rate;
			pwr->dsp2_freq = rate;
			if (!IS_ERR(apu_find_device(VPU2)))
				pwr->dsp3_freq = rate;
		}
		break;
	default:
		return;
	}

	trace_apupwr_pwr(pwr);
	trace_APUSYS_DFS(pwr);
}

void apupw_dbg_dvfs_tag_update(char *gov_name, const char *p_name,
	const char *c_name, u32 opp, ulong freq)
{
	trace_apupwr_dvfs(gov_name, p_name, c_name, opp, freq);
}

void apupw_dbg_rpc_tag_update(struct apu_dev *ad)
{
	u32 result[2]; /* suppse max 2 cgs on 1 engine */
	struct apupwr_tag_rpc *rpc = &pw_tag.rpc;

	if (IS_ERR_OR_NULL(ad->aclk) || IS_ERR_OR_NULL(ad->aclk->cg))
		return;

	if (ad->aclk->cg->clk_num > sizeof(result)) {
		pr_info("[%s] cg numbers %u > array size %d\n",
				__func__, ad->aclk->cg->clk_num, sizeof(result));
		return;
	}

	ad->aclk->ops->cg_status(ad->aclk, result);
	rpc->rpc_intf_rdy = apu_rpc_rdy_value();

	switch (ad->user) {
	case APUCONN:
		rpc->spm_wakeup = apu_spm_wakeup_value();
		rpc->vcore_cg_stat = result[0];
		rpc->conn_cg_stat = result[1];
		break;
	case MDLA0:
		rpc->mdla0_cg_stat = result[0];
		break;
	case MDLA1:
		rpc->mdla1_cg_stat = result[0];
		break;
	case VPU0:
		rpc->vpu0_cg_stat = result[0];
		break;
	case VPU1:
		rpc->vpu1_cg_stat = result[0];
		break;
	case VPU2:
		rpc->vpu2_cg_stat = result[0];
		break;
	default:
		return;
	}

	trace_apupwr_rpc(rpc);
}

struct apupwr_tag *apupw_get_tag(void)
{
	return &pw_tag;
}
EXPORT_SYMBOL_GPL(apupw_get_tag);


