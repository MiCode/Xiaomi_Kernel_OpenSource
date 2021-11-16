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

#ifndef TEEI_TASK_LINK_H
#define TEEI_TASK_LINK_H

int teei_init_task_link(void);
int teei_add_to_task_link(struct list_head *entry);
struct task_entry_struct *teei_get_task_from_link(void);
int is_teei_task_link_empty(void);

int teei_add_to_bdrv_link(struct list_head *entry);
struct bdrv_work_struct *teei_get_bdrv_from_link(void);

#endif  /* end of TEEI_TASK_LINK_H */

