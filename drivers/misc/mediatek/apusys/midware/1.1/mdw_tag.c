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

/* The parameters must aligned with trace_mdw_ap_cmd() */
static void
probe_mdw_cmd(void *data, uint32_t done, pid_t pid, pid_t tgid,
		uint64_t cmd_id,
		uint64_t sc_info,
		char *dev_name,
		uint64_t multi_info,
		uint64_t exec_info,
		uint64_t tcm_info,
		uint32_t boost, uint32_t ip_time,
		int ret)
{
	struct mdw_tag t;

	if (!mdw_tags)
		return;

	t.type = MDW_TAG_CMD;
	t.d.cmd.done = done;
	t.d.cmd.pid = pid;
	t.d.cmd.tgid = tgid;
	t.d.cmd.cmd_id = cmd_id;
	t.d.cmd.sc_info = sc_info;
	strncpy(t.d.cmd.dev_name, dev_name, (MDW_DEV_NAME_SIZE - 1));
	t.d.cmd.multi_info = multi_info;
	t.d.cmd.exec_info = exec_info;
	t.d.cmd.tcm_info = tcm_info;
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
	seq_printf(s, "pid=%d,tgid=%d,cmd_id=0x%llx,sc_info=0x%llx,",
		t->d.cmd.pid, t->d.cmd.tgid,
		t->d.cmd.cmd_id, t->d.cmd.sc_info);
	seq_printf(s, "dev_name=%s,multi_info=0x%llx,exec_info=0x%llx,tcm_info=0x%llx,",
		t->d.cmd.dev_name, t->d.cmd.multi_info,
		t->d.cmd.exec_info, t->d.cmd.tcm_info);
	seq_printf(s, "boost=%u,ip_time=%u,ret=%d\n",
		t->d.cmd.boost,
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

