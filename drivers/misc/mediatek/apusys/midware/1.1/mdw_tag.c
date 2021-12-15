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

#include "apu_tags.h"
#include "apu_tp.h"
#include "mdw_rsc.h"
#include "mdw_tag.h"
#include "mdw_events.h"

static struct apu_tags *mdw_tags;

enum mdw_tag_type {
	MDW_TAG_CMD,
};

/* The parameters must aligned with trace_mdw_cmd() */
static void
probe_mdw_cmd(void *data, uint32_t done, pid_t pid, pid_t tgid,
		uint64_t uid, uint64_t cmd_id,  int sc_idx,
		uint32_t num_sc, int type, char *dev_name, int dev_idx,
		uint32_t pack_id,
		uint32_t multicore_idx, uint32_t exec_core_num,
		uint64_t exec_core_bitmap, unsigned char priority,
		uint32_t soft_limit, uint32_t hard_limit,
		uint32_t exec_time, uint32_t suggest_time,
		unsigned char power_save, uint32_t ctx_id,
		unsigned char tcm_force, uint32_t tcm_usage,
		uint32_t tcm_real_usage, uint32_t boost, uint32_t ip_time,
		int ret)
{
	struct mdw_tag t;

	if (!mdw_tags)
		return;

	t.type = MDW_TAG_CMD;
	t.d.cmd.done = done;
	t.d.cmd.pid = pid;
	t.d.cmd.tgid = tgid;
	t.d.cmd.uid = uid;
	t.d.cmd.cmd_id = cmd_id;
	t.d.cmd.sc_idx = sc_idx;
	t.d.cmd.num_sc = num_sc;
	t.d.cmd.type = type;
	strncpy(t.d.cmd.dev_name, dev_name, (MDW_DEV_NAME_SIZE - 1));
	t.d.cmd.dev_idx = dev_idx;
	t.d.cmd.pack_id = pack_id;
	t.d.cmd.multicore_idx = multicore_idx;
	t.d.cmd.exec_core_num = exec_core_num;
	t.d.cmd.exec_core_bitmap = exec_core_bitmap;
	t.d.cmd.priority = priority;
	t.d.cmd.soft_limit = soft_limit;
	t.d.cmd.hard_limit = hard_limit;
	t.d.cmd.exec_time = exec_time;
	t.d.cmd.suggest_time = suggest_time;
	t.d.cmd.power_save = power_save;
	t.d.cmd.ctx_id = ctx_id;
	t.d.cmd.tcm_force = tcm_force;
	t.d.cmd.tcm_usage = tcm_usage;
	t.d.cmd.tcm_real_usage = tcm_real_usage;
	t.d.cmd.boost = boost;
	t.d.cmd.ip_time = ip_time;
	t.d.cmd.ret = ret;

	apu_tag_add(mdw_tags, &t);
}

static void mdw_tag_seq_cmd(struct seq_file *s, struct mdw_tag *t)
{
	char status[8];

	if (t->d.cmd.done) {
		if (snprintf(status, sizeof(status)-1, "%s", "done") < 0)
			return;
	} else {
		if (snprintf(status, sizeof(status)-1, "%s", "start") < 0)
			return;
	}
	seq_printf(s, "%s,", status);
	seq_printf(s, "pid=%d,tgid=%d,cmd_uid=0x%llx,cmd_id=0x%llx,sc_idx=%d,total_sc=%u,",
		t->d.cmd.pid, t->d.cmd.tgid, t->d.cmd.uid,
		t->d.cmd.cmd_id, t->d.cmd.sc_idx, t->d.cmd.num_sc);
	seq_printf(s, "dev_type=%d,dev_name=%s,dev_idx=%d,pack_id=0x%x,mc_idx=%u,mc_num=%u,mc_bitmap=0x%llx,",
		t->d.cmd.type, t->d.cmd.dev_name, t->d.cmd.dev_idx,
		t->d.cmd.pack_id, t->d.cmd.multicore_idx,
		t->d.cmd.exec_core_num, t->d.cmd.exec_core_bitmap);
	seq_printf(s, "priority=%d,soft_limit=%u,hard_limit=%u,exec_time=%u,suggest_time=%u,power_save=%d,",
		t->d.cmd.priority, t->d.cmd.soft_limit,
		t->d.cmd.hard_limit, t->d.cmd.exec_time,
		t->d.cmd.suggest_time, t->d.cmd.power_save);
	seq_printf(s, "mem_ctx=%u,tcm_force=%d,tcm_usage=0x%x,tcm_teal_usage=0x%x,boost=%u,ip_time=%u,ret=%d\n",
		t->d.cmd.ctx_id, t->d.cmd.tcm_force, t->d.cmd.tcm_usage,
		t->d.cmd.tcm_real_usage, t->d.cmd.boost,
		t->d.cmd.ip_time, t->d.cmd.ret);
}

static int mdw_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct mdw_tag *t = (struct mdw_tag *)tag;

	if (!t)
		return -ENOENT;

	if (t->type == MDW_TAG_CMD)
		mdw_tag_seq_cmd(s, t);

	return 0;
}

static int mdw_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl mdw_tp_tbl[] = {
	{.name = "mdw_cmd", .func = probe_mdw_cmd},
	APU_TP_TBL_END
};

void mdw_tag_show(struct seq_file *s)
{
	apu_tags_seq(mdw_tags, s);
}

int mdw_tag_init(void)
{
	int ret;

	mdw_tags = apu_tags_alloc("mdw", sizeof(struct mdw_tag),
		MDW_TAGS_CNT, mdw_tag_seq, mdw_tag_seq_info, NULL);

	if (!mdw_tags)
		return -ENOMEM;

	ret = apu_tp_init(mdw_tp_tbl);
	if (ret)
		pr_info("%s: unable to register\n", __func__);

	return ret;
}

void mdw_tag_exit(void)
{
	apu_tp_exit(mdw_tp_tbl);
	apu_tags_free(mdw_tags);
}

