// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef GBE_USEDEXT_H
#define GBE_USEDEXT_H

extern void (*gbe_fstb2gbe_poll_fp)(struct hlist_head *list);

struct GBE_BOOST_LIST {
	struct hlist_node hlist;
	char process_name[16];
	char thread_name[16];
	unsigned long long runtime_thrs;
	unsigned long long runtime_percent;
	unsigned long long last_task_runtime;
	unsigned long long now_task_runtime;
	unsigned long long cur_ts;
	unsigned long long last_ts;
	int pid;
	int tid;
	unsigned long long boost_cnt;
};

struct GBE_FSTB_TID_LIST {
	struct hlist_node hlist;
	int tid;
};

static HLIST_HEAD(gbe_boost_list);
static HLIST_HEAD(gbe_fstb_tid_list);

#endif

