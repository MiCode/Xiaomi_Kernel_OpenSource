/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
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

