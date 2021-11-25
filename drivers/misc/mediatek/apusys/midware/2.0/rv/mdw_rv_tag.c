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
#include "mdw_rv.h"
#include "mdw_rv_tag.h"
#include "mdw_rv_events.h"

static struct apu_tags *mdw_rv_tags;

enum mdw_tag_type {
	MDW_TAG_CMD,
};

/* The parameters must aligned with trace_mdw_rv_cmd() */
static void
probe_rv_mdw_cmd(void *data, bool done, pid_t pid, pid_t tgid,
		uint64_t uid, uint64_t kid, uint64_t rvid,
		uint32_t num_subcmds, uint32_t num_cmdbufs,
		uint32_t priority,
		uint32_t softlimit,
		uint32_t pwr_dtime,
		uint64_t sc_rets)
{
	struct mdw_rv_tag t;

	if (!mdw_rv_tags)
		return;

	t.type = MDW_TAG_CMD;
	t.d.cmd.done = done;
	t.d.cmd.pid = pid;
	t.d.cmd.tgid = tgid;
	t.d.cmd.uid = uid;
	t.d.cmd.kid = kid;
	t.d.cmd.rvid = rvid;
	t.d.cmd.num_subcmds = num_subcmds;
	t.d.cmd.num_cmdbufs = num_cmdbufs;
	t.d.cmd.priority = priority;
	t.d.cmd.softlimit = softlimit;
	t.d.cmd.pwr_dtime = pwr_dtime;
	t.d.cmd.sc_rets = sc_rets;

	apu_tag_add(mdw_rv_tags, &t);
}

static void mdw_rv_tag_seq_cmd(struct seq_file *s, struct mdw_rv_tag *t)
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
	seq_printf(s, "pid=%d,tgid=%d,uid=0x%llx,kid=0x%llx,rvid=0x%llx,",
		t->d.cmd.pid, t->d.cmd.tgid,
		t->d.cmd.uid, t->d.cmd.kid, t->d.cmd.rvid);
	seq_printf(s, "num_subcmds=%u,num_cmdbufs=%u,",
		t->d.cmd.num_subcmds, t->d.cmd.num_cmdbufs);
	seq_printf(s, "priority=%u,softlimit=%u,",
		t->d.cmd.priority, t->d.cmd.softlimit);
	seq_printf(s, "pwr_dtime=%u,sc_rets=0x%llx\n",
		t->d.cmd.pwr_dtime,
		t->d.cmd.sc_rets);
}

static int mdw_rv_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct mdw_rv_tag *t = (struct mdw_rv_tag *)tag;

	if (!t)
		return -ENOENT;

	if (t->type == MDW_TAG_CMD)
		mdw_rv_tag_seq_cmd(s, t);

	return 0;
}

static int mdw_rv_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl mdw_rv_tp_tbl[] = {
	{.name = "mdw_rv_cmd", .func = probe_rv_mdw_cmd},
	APU_TP_TBL_END
};

void mdw_rv_tag_show(struct seq_file *s)
{
	apu_tags_seq(mdw_rv_tags, s);
}

int mdw_rv_tag_init(void)
{
	int ret;

	mdw_rv_tags = apu_tags_alloc("mdw", sizeof(struct mdw_rv_tag),
		MDW_TAGS_CNT, mdw_rv_tag_seq, mdw_rv_tag_seq_info, NULL);

	if (!mdw_rv_tags)
		return -ENOMEM;

	ret = apu_tp_init(mdw_rv_tp_tbl);
	if (ret)
		pr_info("%s: unable to register\n", __func__);

	return ret;
}

void mdw_rv_tag_deinit(void)
{
	apu_tp_exit(mdw_rv_tp_tbl);
	apu_tags_free(mdw_rv_tags);
}

