/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_SCHEDULER_H__
#define __MDLA_SCHEDULER_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/list.h>

struct command_entry;


/* platform callback functions */
struct mdla_sched_cb_func {
	void (*split_alloc_cmd_batch)(struct command_entry *ce);
	void (*del_free_cmd_batch)(struct command_entry *ce);
};

struct mdla_sched_cb_func *mdla_sched_plat_cb(void);

void mdla_sched_set_dev_handle_cap(u32 core_id, int cmd_num);
int mdla_sched_get_dev_handle_cap(u32 core_id);
void mdla_sched_set_cmd_prio_lv(int max_lv);
int mdla_sched_get_cmd_prio_lv(void);

#endif /* __MDLA_SCHEDULER_H__ */

