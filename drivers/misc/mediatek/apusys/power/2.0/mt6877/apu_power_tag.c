/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>
#include "apu_power_tag.h"
#include "apu_tags.h"
#include "apu_tp.h"

/* tags */
static struct apu_tags *apupwr_drv_tags;

enum apupwr_tag_type {
	APUPWR_TAG_PWR,
	APUPWR_TAG_RPC,
	APUPWR_TAG_DVFS,
};

/* The parameters must aligned with trace_apupwr_pwr() */
static void
probe_apupwr_pwr(void *data, unsigned int vvpu,
	unsigned int vcore, unsigned int vsram, unsigned int vpu0_freq,
	unsigned int vpu1_freq, unsigned int mdla0_freq,
	unsigned int conn_freq, unsigned int iommu_freq,
	unsigned long long id)
{
	struct apupwr_tag t;

	if (!apupwr_drv_tags)
		return;

	t.type = APUPWR_TAG_PWR;

	t.d.pwr.vvpu = vvpu;
	t.d.pwr.vcore = vcore;
	t.d.pwr.vsram = vsram;
	t.d.pwr.vpu0_freq = vpu0_freq;
	t.d.pwr.vpu1_freq = vpu1_freq;
	t.d.pwr.mdla0_freq = mdla0_freq;
	t.d.pwr.conn_freq = conn_freq;
	t.d.pwr.iommu_freq = iommu_freq;
	t.d.pwr.id = id;

	apu_tag_add(apupwr_drv_tags, &t);
}

/* The parameters must aligned with trace_apupwr_rpc() */
static void
probe_apupwr_rpc(void *data, unsigned int spm_wakeup, unsigned int rpc_intf_rdy,
	unsigned int vcore_cg_stat, unsigned int conn_cg_stat,
	unsigned int conn1_cg_stat, unsigned int vpu0_cg_stat,
	unsigned int vpu1_cg_stat, unsigned int mdla0_cg_stat)
{
	struct apupwr_tag t;

	if (!apupwr_drv_tags)
		return;

	t.type = APUPWR_TAG_RPC;

	t.d.rpc.spm_wakeup = spm_wakeup;
	t.d.rpc.rpc_intf_rdy = rpc_intf_rdy;
	t.d.rpc.vcore_cg_stat = vcore_cg_stat;
	t.d.rpc.conn_cg_stat = conn_cg_stat;
	t.d.rpc.conn1_cg_stat = conn1_cg_stat;
	t.d.rpc.vpu0_cg_stat = vpu0_cg_stat;
	t.d.rpc.vpu1_cg_stat = vpu1_cg_stat;
	t.d.rpc.mdla0_cg_stat = mdla0_cg_stat;

	apu_tag_add(apupwr_drv_tags, &t);
}

/* The parameters must aligned with trace_apupwr_dvfs() */
static void
probe_apupwr_dvfs(void *data, char *log_str)
{
	struct apupwr_tag t;

	if (!apupwr_drv_tags)
		return;

	t.type = APUPWR_TAG_DVFS;

	strncpy(t.d.dvfs.log_str, log_str, (LOG_STR_LEN - 1));

	apu_tag_add(apupwr_drv_tags, &t);
}

static void apupwr_tag_seq_pwr(struct seq_file *s, struct apupwr_tag *t)
{
	seq_printf(s,
		"V_vpu=%d,V_core=%d,V_sram=%d,start_time=",
		t->d.pwr.vvpu, t->d.pwr.vcore,
		t->d.pwr.vsram);

	apu_tags_seq_time(s, t->d.pwr.id);

	seq_printf(s,
		",F_v0=%d,F_v1=%d,F_m0=%d,F_conn=%d,F_iommu=%d\n",
		t->d.pwr.vpu0_freq, t->d.pwr.vpu1_freq,
		t->d.pwr.mdla0_freq, t->d.pwr.conn_freq,
		t->d.pwr.iommu_freq);
}

static void apupwr_tag_seq_rpc(struct seq_file *s, struct apupwr_tag *t)
{
	seq_printf(s, "spm_wp=0x%x,rpc_rdy=0x%x",
	t->d.rpc.spm_wakeup, t->d.rpc.rpc_intf_rdy);
	seq_printf(s, ",vcore_cg=0x%x,conn_cg=0x%x,conn1_cg=0x%x",
	t->d.rpc.vcore_cg_stat, t->d.rpc.conn_cg_stat, t->d.rpc.conn1_cg_stat);
	seq_printf(s, ",v0_cg=0x%x,v1_cg=0x%x,m0_cg=0x%x\n",
	t->d.rpc.vpu0_cg_stat, t->d.rpc.vpu1_cg_stat,
	t->d.rpc.mdla0_cg_stat);
}

static void apupwr_tag_seq_dvfs(struct seq_file *s, struct apupwr_tag *t)
{
	seq_printf(s, "dvfs = %s\n", t->d.dvfs.log_str);
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
	apu_tags_seq(apupwr_drv_tags, s);
}

int apupwr_init_drv_tags(void)
{
	int ret;

	apupwr_drv_tags = apu_tags_alloc("apupwr", sizeof(struct apupwr_tag),
		APUPWR_TAGS_CNT, apupwr_tag_seq, apupwr_tag_seq_info, NULL);

	if (!apupwr_drv_tags)
		return -ENOMEM;

	ret = apu_tp_init(apupwr_tp_tbl);
	if (ret)
		pr_info("%s: unable to register\n", __func__);

	return ret;
}

void apupwr_exit_drv_tags(void)
{
	apu_tp_exit(apupwr_tp_tbl);
	apu_tags_free(apupwr_drv_tags);
}

