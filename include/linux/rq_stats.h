/* Copyright (c) 2011,2013-2014,2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

struct rq_data {
	unsigned long def_timer_jiffies;
	unsigned long def_timer_last_jiffy;
	int64_t def_start_time;
	struct attribute_group *attr_group;
	struct kobject *kobj;
	struct work_struct def_timer_work;
	int init;
};

extern spinlock_t rq_lock;
extern struct rq_data rq_info;
extern struct workqueue_struct *rq_wq;
