/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SIPA_TIMER_TASK_H
#define _SIPA_TIMER_TASK_H

#define SIPA_TIMER_TASK_INVALID_HDL		(8)

int sipa_timer_task_start(uint32_t hdl);
int sipa_timer_task_stop(uint32_t hdl);
int sipa_timer_task_init(void);
void sipa_timer_task_exit(void);

int sipa_timer_task_register(
	uint32_t hdl,
	const char *name,
	uint32_t user_id,
	uint32_t wake_up_interval_ms,
	/* is_first > 0 means first, other means not first */
	int (*process)(int is_first, void *data),
	void *data,
	int OnlyOnce,
	int first_trigger);

int sipa_timer_task_unregister(
	uint32_t hdl,
	uint32_t user_id);

#endif /* _SIPA_TIMER_TASK_H */
