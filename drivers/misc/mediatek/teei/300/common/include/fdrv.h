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
#ifndef __TEEI_FDRV_H__
#define __TEEI_FDRV_H__

#include <linux/list.h>
#include <notify_queue.h>
#include <teei_common.h>

struct teei_fdrv {
	void *buf;
	int buff_size;
	int call_type;
	struct list_head list;
};

/* used by fdrv drivers */
void register_fdrv(struct teei_fdrv *fdrv);

int create_fdrv(struct teei_fdrv *fdrv);
int fdrv_notify(struct teei_fdrv *fdrv);

void teei_handle_fdrv_call(struct NQ_entry *entry);

/* used internally by tz_driver */
int create_all_fdrv(void);
int __call_fdrv(struct fdrv_call_struct *fdrv_ent);

#endif
