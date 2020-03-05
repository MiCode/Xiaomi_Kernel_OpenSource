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
#include "vpu_cmn.h"
#include "vpu_debug.h"
#include "vpu_tag.h"
#include "apu_tags.h"
#include "apu_tp.h"

enum vpu_tag_type {
	VPU_TAG_CMD,
	VPU_TAG_DMP,
	VPU_TAG_WAIT,
};

/* The parameters must aligned with trace_vpu_cmd() */
static void
probe_vpu_cmd(void *data, int core, int prio, char *algo, int cmd,
	int boost, uint64_t start_time, int ret, int algo_ret, int result)
{
	struct vpu_tag t;

	if (!vpu_drv->tags)
		return;

	t.type = VPU_TAG_CMD;
	t.core = core;

	t.d.cmd.prio = prio;
	strncpy(t.d.cmd.algo, algo, (ALGO_NAMELEN - 1));
	t.d.cmd.cmd = cmd;
	t.d.cmd.boost = boost;
	t.d.cmd.start_time = start_time;
	t.d.cmd.ret = ret;
	t.d.cmd.algo_ret = algo_ret;
	t.d.cmd.result = result;

	apu_tag_add(vpu_drv->tags, &t);
}

/* The parameters must aligned with trace_vpu_dmp() */
static void
probe_vpu_dmp(void *data, int core, char *stage, uint32_t pc)
{
	struct vpu_tag t;

	if (!vpu_drv->tags)
		return;

	t.type = VPU_TAG_DMP;
	t.core = core;

	strncpy(t.d.dmp.stage, stage, (STAGE_NAMELEN - 1));
	t.d.dmp.pc = pc;

	apu_tag_add(vpu_drv->tags, &t);
}

/* The parameters must aligned with trace_vpu_wait() */
static void
probe_vpu_wait(void *data, int core, uint32_t donest, uint32_t info00,
	uint32_t info25, uint32_t pc)
{
	struct vpu_tag t;

	if (!vpu_drv->tags)
		return;

	t.type = VPU_TAG_WAIT;
	t.core = core;

	t.d.wait.donest = donest;
	t.d.wait.info00 = info00;
	t.d.wait.info25 = info25;
	t.d.wait.pc = pc;

	apu_tag_add(vpu_drv->tags, &t);
}


static void vpu_tag_seq_cmd(struct seq_file *s, struct vpu_tag *t)
{
	seq_printf(s, "vpu%d,prio=%d,%s,boost=",
		t->core, t->d.cmd.prio, t->d.cmd.algo);
	vpu_seq_boost(s, t->d.cmd.boost);
	seq_printf(s, ",cmd=%xh,start_time=", t->d.cmd.cmd);
	apu_tags_seq_time(s, t->d.cmd.start_time);
	seq_printf(s, ",ret=%d,alg_ret=%d,result=%d\n",
		t->d.cmd.ret, t->d.cmd.algo_ret, t->d.cmd.result);
}

static void vpu_tag_seq_dmp(struct seq_file *s, struct vpu_tag *t)
{
	seq_printf(s, "vpu%d,dump=%s,pc=0x%x\n",
		t->core, t->d.dmp.stage, t->d.dmp.pc);
}

static void vpu_tag_seq_wait(struct seq_file *s, struct vpu_tag *t)
{
	seq_printf(s, "vpu%d,donest=0x%x,info00=0x%x,info25=0x%x,pc=0x%x\n",
		t->core, t->d.wait.donest, t->d.wait.info00,
		t->d.wait.info25, t->d.wait.pc);
}

static int vpu_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct vpu_tag *t = (struct vpu_tag *)tag;

	if (!t)
		return -ENOENT;

	if (t->type == VPU_TAG_CMD)
		vpu_tag_seq_cmd(s, t);
	else if (t->type == VPU_TAG_DMP)
		vpu_tag_seq_dmp(s, t);
	else if (t->type == VPU_TAG_WAIT)
		vpu_tag_seq_wait(s, t);

	return 0;
}

static int vpu_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl vpu_tp_tbl[] = {
	{.name = "vpu_cmd", .func = probe_vpu_cmd},
	{.name = "vpu_dmp", .func = probe_vpu_dmp},
	{.name = "vpu_wait", .func = probe_vpu_wait},
	APU_TP_TBL_END
};

int vpu_init_drv_tags(void)
{
	int ret;

	vpu_drv->tags = apu_tags_alloc("vpu", sizeof(struct vpu_tag),
		VPU_TAGS_CNT, vpu_tag_seq, vpu_tag_seq_info, vpu_drv);

	if (!vpu_drv->tags)
		return -ENOMEM;

	ret = apu_tp_init(vpu_tp_tbl);
	if (!ret)
		pr_info("%s: unable to register\n", __func__);

	return ret;
}

void vpu_exit_drv_tags(void)
{
	apu_tp_exit(vpu_tp_tbl);
	apu_tags_free(vpu_drv->tags);
}

