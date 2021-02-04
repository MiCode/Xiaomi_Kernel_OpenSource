/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __HW_BREAKPOINT_H
#define __HW_BREAKPOINT_H

typedef int (*wp_handler) (unsigned long addr);

struct wp_event {
	unsigned long virt;
	unsigned long phys;
	int type;
	wp_handler handler;
	int in_use;
	int auto_disable;
};

#define WP_EVENT_TYPE_READ 1
#define WP_EVENT_TYPE_WRITE 2
#define WP_EVENT_TYPE_ALL 3

#define init_wp_event(__e, __v, __p, __t, __h)   \
	do {    \
		(__e)->virt = (phys_addr_t)(__v);    \
		(__e)->phys = (phys_addr_t)(__p);    \
		(__e)->type = (__t);    \
		(__e)->handler = (wp_handler)(__h);    \
		(__e)->auto_disable = 0;    \
	} while (0)

#define auto_disable_wp(__e)   \
	{\
		(__e)->auto_disable = 1; \
	}

extern int add_hw_watchpoint(struct wp_event *wp_event);
extern int del_hw_watchpoint(struct wp_event *wp_event);
extern void reset_watchpoint(void);
#endif				/* !__HW_BREAKPOINT_H */
