/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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

