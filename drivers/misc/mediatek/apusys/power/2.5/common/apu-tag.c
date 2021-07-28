// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>
#include "apupw_tag.h"
#include "apu_tags.h"
#include "apu_tp.h"
#include "apu_trace.h"
#include "apu_common.h"

/* tags */
static struct apu_tags *apu_drv_tag;
static struct apupwr_tag *apupw_tag;
enum apupwr_tag_type {
	APUPWR_TAG_PWR,
	APUPWR_TAG_RPC,
	APUPWR_TAG_DVFS,
};

/* The parameters must aligned with trace_apupwr_pwr() */
static void
probe_apupwr_pwr(void *data, struct apupwr_tag_pwr *pw)
{

	if (!apu_drv_tag)
		return;

	apupw_tag->type = APUPWR_TAG_PWR;
	apu_tag_add(apu_drv_tag, apupw_tag);
}

/* The parameters must aligned with trace_apupwr_rpc() */
static void
probe_apupwr_rpc(void *data, struct apupwr_tag_rpc *rpc)
{
	if (!apu_drv_tag)
		return;

	apupw_tag->type = APUPWR_TAG_RPC;
	apu_tag_add(apu_drv_tag, apupw_tag);
}

/* The parameters must aligned with trace_apupwr_dvfs() */
static void
probe_apupwr_dvfs(void *data, char *gov_name, const char *p_name,
		  const char *c_name, u32 opp, ulong freq)
{
	if (!apu_drv_tag)
		return;

	apupw_tag->type = APUPWR_TAG_DVFS;
	apupw_tag->dvfs.gov_name = gov_name;
	apupw_tag->dvfs.p_name = p_name;
	apupw_tag->dvfs.c_name = c_name;
	apupw_tag->dvfs.opp = opp;
	apupw_tag->dvfs.freq = freq;
	apu_tag_add(apu_drv_tag, apupw_tag);
}

static void apupwr_tag_seq_pwr(struct seq_file *s, struct apupwr_tag *t)
{
	seq_printf(s,
		"V_vpu=%d,V_mdla=%d,V_core=%d,V_sram=%d,start_time=",
		TOMV(t->pwr.vvpu), TOMV(t->pwr.vmdla),
		TOMV(t->pwr.vcore), TOMV(t->pwr.vsram));
	seq_printf(s,
		",F_v0=%d,F_v1=%d,F_v2=%d,F_m0=%d,F_m1=%d,F_conn=%d,F_ipuif=%d,F_iommu=%d\n",
		TOMHZ(t->pwr.dsp1_freq), TOMHZ(t->pwr.dsp2_freq), TOMHZ(t->pwr.dsp3_freq),
		TOMHZ(t->pwr.dsp5_freq), TOMHZ(t->pwr.dsp6_freq), TOMHZ(t->pwr.dsp_freq),
		TOMHZ(t->pwr.ipuif_freq), TOMHZ(t->pwr.dsp7_freq));
}

static void apupwr_tag_seq_rpc(struct seq_file *s, struct apupwr_tag *t)
{
	seq_printf(s, "spm_wp=0x%x,rpc_rdy=0x%x",
	t->rpc.spm_wakeup, t->rpc.rpc_intf_rdy);
	seq_printf(s, ",vcore_cg=0x%x,conn_cg=0x%x",
	t->rpc.vcore_cg_stat, t->rpc.conn_cg_stat);
	seq_printf(s, ",v0_cg=0x%x,v1_cg=0x%x,v2_cg=0x%x,m0_cg=0x%x,m1_cg=0x%x\n",
		   t->rpc.vpu0_cg_stat, t->rpc.vpu1_cg_stat, t->rpc.vpu2_cg_stat,
		   t->rpc.mdla0_cg_stat, t->rpc.mdla1_cg_stat);
}

static void apupwr_tag_seq_dvfs(struct seq_file *s, struct apupwr_tag *t)
{
	seq_printf(s, "[%s] %s->%s[%d/%lu]\n",
		   t->dvfs.gov_name, t->dvfs.p_name,
		   t->dvfs.c_name, t->dvfs.opp, t->dvfs.freq);
}

static int apupwr_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct apupwr_tag *t = (struct apupwr_tag *)tag;

	if (!t)
		return -ENOENT;

	if (t->type == APUPWR_TAG_PWR)
		apupwr_tag_seq_pwr(s, t);
	else if (t->type == APUPWR_TAG_RPC)
		apupwr_tag_seq_rpc(s, t);
	else if (t->type == APUPWR_TAG_DVFS)
		apupwr_tag_seq_dvfs(s, t);

	return 0;
}

static int apupwr_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl apupwr_tp_tbl[] = {
	{.name = "apupwr_pwr", .func = probe_apupwr_pwr},
	{.name = "apupwr_rpc", .func = probe_apupwr_rpc},
	{.name = "apupwr_dvfs", .func = probe_apupwr_dvfs},
	APU_TP_TBL_END
};

void apupwr_tags_show(struct seq_file *s)
{
	apu_tags_seq(apu_drv_tag, s);
}

int apupwr_init_tags(struct apusys_core_info *info)
{
	int ret;

	apu_drv_tag = apu_tags_alloc("apupwr", sizeof(struct apupwr_tag),
		APUPWR_TAGS_CNT, apupwr_tag_seq, apupwr_tag_seq_info, NULL);

	if (!apu_drv_tag)
		return -ENOMEM;

	ret = apu_tp_init(apupwr_tp_tbl);
	if (ret)
		pr_info("%s: unable to register\n", __func__);

	apupw_tag = apupw_get_tag();
	return ret;
}

void apupwr_exit_tags(void)
{
	apu_tp_exit(apupwr_tp_tbl);
	apu_tags_free(apu_drv_tag);
}
