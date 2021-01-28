/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SWITCH_QUEUE_H
#define SWITCH_QUEUE_H

struct task_entry_struct {
	unsigned long long work_type;
	unsigned long long x0;
	unsigned long long x1;
	unsigned long long x2;
	unsigned long long x3;
	struct list_head c_link;
};

int add_work_entry(unsigned long long work_type, unsigned long long x0,
		unsigned long long x1, unsigned long long x2,
		unsigned long long x3);

int teei_notify_switch_fn(void);
int init_teei_switch_comp(void);
int teei_switch_fn(void *work);

#endif  /* end of SWITCH_QUEUE_H */
