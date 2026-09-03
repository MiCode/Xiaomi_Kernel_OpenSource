// SPDX-License-Identifier: GPL-2.0-only
/*
 * kprobe_filemap.c
 *
 * Print inode info when trace filemap.
 *
 * Copyright (C) 2022-2023 UNISOC, Inc.
 *             https://www.unisoc.com/
 *
 * Module_author: Hongyu.Jin@unisoc.com Dongliang.cui@unisoc.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <trace/hooks/sched.h>
#include <linux/stacktrace.h>
#include <linux/dcache.h>
#include <trace/events/filemap.h>
#include <linux/tracepoint.h>
#include <linux/jump_label.h>

static struct tracepoint *tp_add, *tp_del;

enum sprd_trace_type {
	PAGE_ADD,
	PAGE_DEL,
};

enum sprd_tp_func_state {
	TP_FUNC_0,
	TP_FUNC_1,
	TP_FUNC_2,
	TP_FUNC_N,
};

static struct kprobe sprd_tracepoint_add_func_kp = {
	.symbol_name	= "tracepoint_probe_register",
};

static enum sprd_tp_func_state sprd_nr_func_state(const struct tracepoint_func *tp_funcs)
{
	if (!tp_funcs)
		return TP_FUNC_0;
	if (!tp_funcs[1].func)
		return TP_FUNC_1;
	if (!tp_funcs[2].func)
		return TP_FUNC_2;
	return TP_FUNC_N;	/* 3 or more */
}

static void sprd_mm_filemap_inode_info(struct page *page, enum sprd_trace_type type)
{
	struct inode *inode = page->mapping->host;
	struct dentry *dentry;
	char *type_name;
	dev_t s_dev;
	unsigned long flags;

	if (type == PAGE_ADD)
		type_name = "page_cache_add";
	else
		type_name = "page_cache_delete";

	if (!spin_trylock_irqsave(&inode->i_lock, flags))
		return;

	hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
		if (!dentry)
			continue;

		if (inode->i_sb)
			s_dev = inode->i_sb->s_dev;
		else
			s_dev = inode->i_rdev;

		if (dentry->d_name.name)
			trace_printk("%s: dev %d:%d ino %lx name %s\n",
				     type_name, MAJOR(s_dev), MINOR(s_dev), inode->i_ino,
				     dentry->d_name.name);
	}

	spin_unlock_irqrestore(&inode->i_lock, flags);
}

static void sprd_tracepoint_get(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	struct tracepoint *tracepoint = (struct tracepoint *)(regs->regs[0]);
	const char *name = tracepoint->name;

	if (!strcmp(name, "mm_filemap_delete_from_page_cache"))
		tp_del = tracepoint;

	if (!strcmp(name, "mm_filemap_add_to_page_cache"))
		tp_add = tracepoint;
}

static void sprd_del_from_page_cache_info_print(void *data, struct page *page)
{
	if (sprd_nr_func_state(tp_del->funcs) > TP_FUNC_1)
		sprd_mm_filemap_inode_info(page, PAGE_DEL);
}

static void sprd_add_to_page_cache_info_print(void *data, struct page *page)
{
	if (sprd_nr_func_state(tp_add->funcs) > TP_FUNC_1)
		sprd_mm_filemap_inode_info(page, PAGE_ADD);
}

static void sprd_register_exit(void)
{
	unregister_trace_mm_filemap_delete_from_page_cache(sprd_del_from_page_cache_info_print,
							   NULL);
	unregister_trace_mm_filemap_add_to_page_cache(sprd_add_to_page_cache_info_print, NULL);
	unregister_kprobe(&sprd_tracepoint_add_func_kp);
}

static int __init sprd_filemap_kprobe_init(void)
{
	int ret;

	sprd_tracepoint_add_func_kp.post_handler = sprd_tracepoint_get;
	ret = register_kprobe(&sprd_tracepoint_add_func_kp);
	if (ret < 0) {
		pr_err("Register_kretprobe failed, returned %d\n", ret);
		return ret;
	}

	register_trace_mm_filemap_delete_from_page_cache(sprd_del_from_page_cache_info_print, NULL);
	register_trace_mm_filemap_add_to_page_cache(sprd_add_to_page_cache_info_print, NULL);

	if (!tp_add || !tp_del) {
		sprd_register_exit();
		pr_err("Get tracepoint failed, skip insmod kprobe_filemap.\n");
		return -1;
	}

	return 0;
}

static void __exit sprd_filemap_kprobe_exit(void)
{
	sprd_register_exit();
}

module_init(sprd_filemap_kprobe_init)
module_exit(sprd_filemap_kprobe_exit)
MODULE_LICENSE("GPL");
