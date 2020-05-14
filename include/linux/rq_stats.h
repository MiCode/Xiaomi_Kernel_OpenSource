/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2011,2013-2014,2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#ifdef CONFIG_QCOM_RUN_QUEUE_STATS

struct rq_data {
	unsigned long def_timer_jiffies;
	unsigned long def_timer_last_jiffy;
	int64_t def_start_time;
	struct attribute_group *attr_group;
	struct kobject *kobj;
	struct work_struct def_timer_work;
	int init;
};

extern struct rq_data rq_info;
extern struct workqueue_struct *rq_wq;
extern spinlock_t rq_lock;

#endif
