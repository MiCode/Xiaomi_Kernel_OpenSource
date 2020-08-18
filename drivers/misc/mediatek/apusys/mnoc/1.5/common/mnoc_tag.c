// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */

#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>
#include "mnoc_option.h"

#ifdef MNOC_TAG_TP
#include "apu_tags.h"
#include "apu_tp.h"
#include "mnoc_tag.h"

/* tags */
static struct apu_tags *mnoc_drv_tags;

enum mnoc_tag_type {
	MNOC_TAG_EXCEP,
};

/* The parameters must aligned with trace_mnoc_excep() */
static void
probe_mnoc_excep(void *data, unsigned int rt_id, unsigned int sw_irq,
	unsigned int mni_qos_irq, unsigned int addr_dec_err,
	unsigned int mst_parity_err, unsigned int mst_misro_err,
	unsigned int mst_crdt_err, unsigned int slv_parity_err,
	unsigned int slv_misro_err, unsigned int slv_crdt_err,
	unsigned int req_misro_err, unsigned int rsp_misro_err,
	unsigned int req_to_err, unsigned int rsp_to_err,
	unsigned int req_cbuf_err, unsigned int rsp_cbuf_err,
	unsigned int req_crdt_err, unsigned int rsp_crdt_err)
{
	struct mnoc_tag t;

	if (!mnoc_drv_tags)
		return;

	t.type = MNOC_TAG_EXCEP;

	t.d.excep.rt_id = rt_id;
	t.d.excep.sw_irq = sw_irq;
	t.d.excep.mni_qos_irq = mni_qos_irq;
	t.d.excep.addr_dec_err = addr_dec_err;
	t.d.excep.mst_parity_err = mst_parity_err;
	t.d.excep.mst_misro_err = mst_misro_err;
	t.d.excep.mst_crdt_err = mst_crdt_err;
	t.d.excep.slv_parity_err = slv_parity_err;
	t.d.excep.slv_misro_err = slv_misro_err;
	t.d.excep.slv_crdt_err = slv_crdt_err;
	t.d.excep.req_misro_err = req_misro_err;
	t.d.excep.rsp_misro_err = rsp_misro_err;
	t.d.excep.req_to_err = req_to_err;
	t.d.excep.rsp_to_err = rsp_to_err;
	t.d.excep.req_cbuf_err = req_cbuf_err;
	t.d.excep.rsp_cbuf_err = rsp_cbuf_err;
	t.d.excep.req_crdt_err = req_crdt_err;
	t.d.excep.rsp_crdt_err = rsp_crdt_err;

	apu_tag_add(mnoc_drv_tags, &t);
}

static void mnoc_tag_seq_excep(struct seq_file *s, struct mnoc_tag *t)
{
	seq_printf(s,
		"RT%d: sw_irq=0x%x,mni_qos_irq=0x%x,",
		t->d.excep.rt_id, t->d.excep.sw_irq,
		t->d.excep.mni_qos_irq);
	seq_printf(s,
		"addr_dec_err=0x%x,mst_parity_err=0x%x,mst_misro_err=0x%x,",
		t->d.excep.addr_dec_err, t->d.excep.mst_parity_err,
		t->d.excep.mst_misro_err);
	seq_printf(s,
		"mst_crdt_err=0x%x,slv_parity_err=0x%x,slv_misro_err=0x%x,",
		t->d.excep.mst_crdt_err, t->d.excep.slv_parity_err,
		t->d.excep.slv_misro_err);
	seq_printf(s,
		"slv_crdt_err=0x%x,req_misro_err=0x%x,rsp_misro_err=0x%x,",
		t->d.excep.slv_crdt_err, t->d.excep.req_misro_err,
		t->d.excep.rsp_misro_err);
	seq_printf(s,
		"req_to_err=0x%x,rsp_to_err=0x%x,req_cbuf_err=0x%x,",
		t->d.excep.req_to_err, t->d.excep.rsp_to_err,
		t->d.excep.req_cbuf_err);
	seq_printf(s,
		"rsp_cbuf_err=0x%x,req_crdt_err=0x%x,rsp_crdt_err=0x%x\n",
		t->d.excep.rsp_cbuf_err, t->d.excep.req_crdt_err,
		t->d.excep.rsp_crdt_err);
}

static int mnoc_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct mnoc_tag *t = (struct mnoc_tag *)tag;

	if (!t)
		return -ENOENT;

	if (t->type == MNOC_TAG_EXCEP)
		mnoc_tag_seq_excep(s, t);

	return 0;
}

static int mnoc_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl mnoc_tp_tbl[] = {
	{.name = "mnoc_excep", .func = probe_mnoc_excep},
	APU_TP_TBL_END
};

void mnoc_tags_show(struct seq_file *s)
{
	apu_tags_seq(mnoc_drv_tags, s);
}

int mnoc_init_drv_tags(void)
{
	int ret;

	mnoc_drv_tags = apu_tags_alloc("mnoc", sizeof(struct mnoc_tag),
		MNOC_TAGS_CNT, mnoc_tag_seq, mnoc_tag_seq_info, NULL);

	if (!mnoc_drv_tags)
		return -ENOMEM;

	ret = apu_tp_init(mnoc_tp_tbl);
	if (ret)
		pr_info("%s: unable to register\n", __func__);

	return ret;
}

void mnoc_exit_drv_tags(void)
{
	apu_tp_exit(mnoc_tp_tbl);
	apu_tags_free(mnoc_drv_tags);
}

#endif /* MNOC_TAG_TP */
